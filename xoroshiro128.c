#include "xoroshiro128.h"
#include <linux/types.h>

static u64 s[2];
static inline u64 rotl(const u64 x, int k)
{
    return (x << k) | (x >> (64 - k));
}

void seed(u64 s0, u64 s1)
{
    s[0] = s0;
    s[1] = s1;
}

uint64_t xoro_next(void)
{
    const u64 s0 = s[0];
    uint64_t s1 = s[1];
    const u64 result = s0 + s1;

    s1 ^= s0;
    s[0] = rotl(s0, 24) ^ s1 ^ (s1 << 16);  // a, b
    s[1] = rotl(s1, 37);                    // c

    return result;
}

void jump(void)
{
    static const u64 JUMP[] = {0xdf900294d8f554a5, 0x170865df4b3201fc};

    u64 s0 = 0;
    u64 s1 = 0;
    int i, b;
    for (i = 0; i < sizeof JUMP / sizeof *JUMP; i++) {
        for (b = 0; b < 64; b++) {
            if (JUMP[i] & (u64) (1) << b) {
                s0 ^= s[0];
                s1 ^= s[1];
            }
            xoro_next();
        }
    }

    s[0] = s0;
    s[1] = s1;
}

void xoro_init(void)
{
    seed(314159265, 1618033989);
}
