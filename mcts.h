#pragma once

#define ITERATIONS 100000
#define EXPLORATION_FACTOR 1U << (frac_bits - 1)

int mcts(char *table, char player);
