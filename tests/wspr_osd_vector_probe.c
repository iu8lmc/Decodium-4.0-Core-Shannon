/* Compare WSPR OSD results between builds on the same host. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>

void wsprd_osd_decode(float*, unsigned char*, int*, unsigned char*, int*, float*);

static uint32_t state = 20261115u;
static uint32_t next_random(void)
{
    state ^= state << 13;
    state ^= state >> 17;
    state ^= state << 5;
    return state;
}

int main(void)
{
    for (int trial = 0; trial < 180; ++trial) {
        float samples[162];
        unsigned char mask[162], codeword[162];
        for (int i = 0; i < 162; ++i) {
            samples[i] = ((int)(next_random() % 25401) - 12700) / 100.0f;
            mask[i] = (trial % 3 == 0 && next_random() % 20 == 0) ? 1 : 0;
        }
        int depth = trial % 6, nhard = -1;
        float dmin = 0.0f;
        uint32_t distance_bits;
        wsprd_osd_decode(samples, mask, &depth, codeword, &nhard, &dmin);
        memcpy(&distance_bits, &dmin, sizeof distance_bits);
        printf("%d %d %d %08x ", trial, depth, nhard, distance_bits);
        for (int i = 0; i < 162; ++i) printf("%02x", codeword[i]);
        putchar('\n');
    }
    return 0;
}
