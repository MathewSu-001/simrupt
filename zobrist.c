#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>

#include "xoroshiro128.h"
#include "zobrist.h"

u64 zobrist_table[N_GRIDS][2];

#define HASH(key) ((key) % HASH_TABLE_SIZE)

static struct hlist_head *hash_table;

void zobrist_init(void)
{
    int i;
    xoro_init();
    for (i = 0; i < N_GRIDS; i++) {
        zobrist_table[i][0] = xoro_next();
        zobrist_table[i][1] = xoro_next();
        jump();
    }
    hash_table =
        kmalloc(sizeof(struct hlist_head) * HASH_TABLE_SIZE, GFP_KERNEL);
    if (!hash_table) {
        pr_info("simrupt: Failed to allocate space for hash_table\n");
        return;
    }
    for (i = 0; i < HASH_TABLE_SIZE; i++)
        INIT_HLIST_HEAD(&hash_table[i]);
}

zobrist_entry_t *zobrist_get(u64 key)
{
    unsigned long long hash_key = HASH(key);

    if (hlist_empty(&hash_table[hash_key]))
        return NULL;

    zobrist_entry_t *entry = NULL;

    hlist_for_each_entry (entry, &hash_table[hash_key], ht_list)

    {
        if (entry->key == key)
            return entry;
    }
    return NULL;
}

void zobrist_put(u64 key, int score, int move)
{
    unsigned long long hash_key = HASH(key);
    zobrist_entry_t *new_entry = kmalloc(sizeof(zobrist_entry_t), GFP_KERNEL);
    // assert(new_entry);
    new_entry->key = key;
    new_entry->move = move;
    new_entry->score = score;
    hlist_add_head(&new_entry->ht_list, &hash_table[hash_key]);
}

void zobrist_clear(void)
{
    for (int i = 0; i < HASH_TABLE_SIZE; i++) {
        while (!hlist_empty(&hash_table[i])) {
            zobrist_entry_t *entry;
            entry = hlist_entry(hash_table[i].first, zobrist_entry_t, ht_list);
            hlist_del(&entry->ht_list);
            kfree(entry);
        }
        INIT_HLIST_HEAD(&hash_table[i]);
    }
}

MODULE_LICENSE("GPL");
