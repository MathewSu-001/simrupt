#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/slab.h>
#include <linux/string.h>

#include "game.h"
#include "mcts.h"
#include "util.h"

#define frac_bits 16

struct node {
    int move;
    char player;
    int n_visits;
    unsigned long score;
    struct node *parent;
    struct node *children[N_GRIDS];
};

static struct node *new_node(int move, char player, struct node *parent)
{
    struct node *node = kmalloc(sizeof(struct node), GFP_KERNEL);
    node->move = move;
    node->player = player;
    node->n_visits = 0;
    node->score = 0;
    node->parent = parent;
    memset(node->children, 0, sizeof(node->children));
    return node;
}

static void free_node(struct node *node)
{
    for (int i = 0; i < N_GRIDS; i++)
        if (node->children[i])
            free_node(node->children[i]);
    kfree(node);
}

static unsigned long fixed_mul(unsigned long a, unsigned long b)
{
    unsigned long result;

    result = a * b;
    /* Rounding; mid values are rounded up*/
    result += (1U << (frac_bits - 1));

    return result >> frac_bits;
}

static unsigned long fixed_div(unsigned long a, unsigned long b)
{
    unsigned long result = a << frac_bits;

    /* Rounding: mid values are rounded up. */
    result += (b >> 1);

    return result / b;
}

static unsigned long fixed_log(unsigned long n)
{
    unsigned long result = 0;
    unsigned long tmp = fixed_div(n - (1U << frac_bits), n + (1U << frac_bits));
    unsigned long ratio = fixed_mul(tmp, tmp);

    int k = 0;
    while (k < 100) {
        unsigned long ttmp = fixed_div(tmp, (2 * k + 1) << frac_bits);
        tmp = fixed_mul(tmp, ratio);
        k++;

        result += ttmp;
    }

    return result << 1;
}

static unsigned long fixed_sqrt(unsigned long x)
{
    if (!x || x == (1U << frac_bits))
        return x;

    unsigned long z = 0;
    unsigned long m = 1UL << ((63 - __builtin_clz(x)) & ~1UL);
    for (; m; m >>= 2) {
        unsigned long b = z + m;
        z >>= 1;
        if (x >= b) {
            x -= b;
            z += m;
        }
    }
    z <<= (frac_bits >> 1);
    return z;
}

static inline unsigned long uct_score(int n_total,
                                      int n_visits,
                                      unsigned long score)
{
    if (n_visits == 0)
        return (~0U);

    unsigned long f_sqrt =
        fixed_sqrt(fixed_div(fixed_log((unsigned long) n_total << frac_bits),
                             (unsigned long) n_visits));
    unsigned long tmp = fixed_mul((unsigned long) EXPLORATION_FACTOR, f_sqrt);

    return fixed_div(score, (unsigned long) n_visits) + tmp;
}

static struct node *select_move(struct node *node)
{
    struct node *best_node = NULL;
    unsigned long best_score = 0;
    for (int i = 0; i < N_GRIDS; i++) {
        if (!node->children[i])
            continue;
        unsigned long score =
            uct_score(node->n_visits, node->children[i]->n_visits,
                      node->children[i]->score);
        if (score > best_score) {
            best_score = score;
            best_node = node->children[i];
        }
    }

    if (!best_node) {
        while (1) {
            unsigned long rand_num;
            get_random_bytes(&rand_num, sizeof(rand_num));
            best_node = node->children[rand_num % N_GRIDS];
            if (best_node)
                break;
        }
    }

    return best_node;
}

static unsigned long simulate(char *table, char player)
{
    char current_player = player;
    char temp_table[N_GRIDS];
    memcpy(temp_table, table, N_GRIDS);
    while (1) {
        char win;
        int *moves = available_moves(temp_table);
        if (moves[0] == -1) {
            kfree(moves);
            break;
        }
        int n_moves = 0;
        while (n_moves < N_GRIDS && moves[n_moves] != -1)
            ++n_moves;
        unsigned long rand_num;
        get_random_bytes(&rand_num, sizeof(rand_num));
        int move = moves[rand_num % n_moves];
        kfree(moves);
        temp_table[move] = current_player;
        if ((win = check_win(temp_table)) != ' ')
            return calculate_win_value(win, player);
        current_player ^= 'O' ^ 'X';
    }
    return 1U << (frac_bits - 1);
}

static void backpropagate(struct node *node, unsigned long score)
{
    while (node) {
        node->n_visits++;
        node->score += score;
        node = node->parent;
        score = (1U << frac_bits) - score;
    }
}

static void expand(struct node *node, char *table)
{
    int *moves = available_moves(table);
    int n_moves = 0;
    while (n_moves < N_GRIDS && moves[n_moves] != -1)
        ++n_moves;
    for (int i = 0; i < n_moves; i++) {
        node->children[i] = new_node(moves[i], node->player ^ 'O' ^ 'X', node);
    }
    kfree(moves);
}

int mcts(char *table, char player)
{
    char win;
    struct node *root = new_node(-1, player, NULL);
    for (int i = 0; i < ITERATIONS; i++) {
        struct node *node = root;
        char temp_table[N_GRIDS];
        memcpy(temp_table, table, N_GRIDS);
        while (1) {
            if ((win = check_win(temp_table)) != ' ') {
                unsigned long score =
                    calculate_win_value(win, node->player ^ 'O' ^ 'X');
                backpropagate(node, score);
                break;
            }
            if (node->n_visits == 0) {
                unsigned long score = simulate(temp_table, node->player);
                backpropagate(node, score);
                break;
            }
            if (node->children[0] == NULL)
                expand(node, temp_table);
            node = select_move(node);
            // assert(node);
            temp_table[node->move] = node->player ^ 'O' ^ 'X';
        }
    }
    struct node *best_node = NULL;
    int most_visits = -1;
    for (int i = 0; i < N_GRIDS; i++) {
        if (root->children[i] && root->children[i]->n_visits > most_visits) {
            most_visits = root->children[i]->n_visits;
            best_node = root->children[i];
        }
    }

    int best_move;
    if (best_node) {
        best_move = best_node->move;
    } else {
        best_move = -1;  // or some other default value or error code
    }
    free_node(root);
    return best_move;
}

MODULE_LICENSE("GPL");