#include "bench_common.h"
#include "bench_suite.h"

#include "mnist_fc_data.h"
#include "xil_cache.h"

static inline int16_t wblk_get(const int16_t *blocks, int blocks_per_row, int row, int col)
{
    const int tile = MNIST_FC_TILE;
    const int rb = row / tile;
    const int cb = col / tile;
    const int ri = row % tile;
    const int ci = col % tile;
    const int block_idx = rb * blocks_per_row + cb;
    const int block_base = block_idx * tile * tile;
    return blocks[block_base + ri * tile + ci];
}

void bench_mnist_fc(void)
{
    xil_printf("\n\r========================================\n\r");
    xil_printf("  Benchmark 6: MNIST FC (ARM blocked vs fast)\n\r");
    xil_printf("========================================\n\r");
    xil_printf("  Note: DCache enabled for this benchmark\n\r");

    Xil_DCacheEnable();

    const int samples = MNIST_FC_SAMPLE_COUNT;
    const int in_dim = MNIST_FC_IN_DIM;
    const int hid_dim = MNIST_FC_HIDDEN_DIM;
    const int out_dim = MNIST_FC_OUT_DIM;

    const int w1_blocks_per_row = hid_dim / MNIST_FC_TILE;
    const int w2_padded_cols = ((out_dim + MNIST_FC_TILE - 1) / MNIST_FC_TILE) * MNIST_FC_TILE;
    const int w2_blocks_per_row = w2_padded_cols / MNIST_FC_TILE;

    const float s1 = MNIST_FC_X_SCALE * MNIST_FC_W1_SCALE;
    const float s2 = MNIST_FC_W2_SCALE;
    const float b2_scale = (MNIST_FC_W2_SCALE / 127.0f);

    int pass_block = 0;
    int pass_fast = 0;
    float a1[MNIST_FC_HIDDEN_DIM];

    uint64_t t0 = timer_now();
    for (int n = 0; n < samples; n++) {
        for (int h = 0; h < hid_dim; h++) {
            int32_t acc = mnist_fc_b1_q[h];
            const int x_base = n * in_dim;
            for (int i = 0; i < in_dim; i++) {
                const int16_t wq = wblk_get(mnist_fc_w1_blocks, w1_blocks_per_row, i, h);
                acc += (int32_t)mnist_fc_x_q[x_base + i] * (int32_t)wq;
            }
            if (acc < 0) acc = 0;
            a1[h] = (float)acc * s1;
        }

        int pred = 0;
        float best = -1e30f;
        for (int o = 0; o < out_dim; o++) {
            float logit = (float)mnist_fc_b2_q[o] * b2_scale;
            for (int h = 0; h < hid_dim; h++) {
                const int16_t wq = wblk_get(mnist_fc_w2_blocks, w2_blocks_per_row, h, o);
                logit += a1[h] * ((float)wq * s2);
            }
            if (o == 0 || logit > best) {
                best = logit;
                pred = o;
            }
        }
        if (pred == (int)mnist_fc_y[n])
            pass_block++;
    }
    uint64_t t1 = timer_now();

    uint64_t t2 = timer_now();
    for (int n = 0; n < samples; n++) {
        for (int h = 0; h < hid_dim; h++) {
            int32_t acc = mnist_fc_b1_q[h];
            const int x_base = n * in_dim;
            for (int i = 0; i < in_dim; i++) {
                const int8_t wq = mnist_fc_w1_q[i * hid_dim + h];
                acc += (int32_t)mnist_fc_x_q[x_base + i] * (int32_t)wq;
            }
            if (acc < 0) acc = 0;
            a1[h] = (float)acc * s1;
        }

        int pred = 0;
        float best = -1e30f;
        for (int o = 0; o < out_dim; o++) {
            float logit = (float)mnist_fc_b2_q[o] * b2_scale;
            for (int h = 0; h < hid_dim; h++) {
                const int8_t wq = mnist_fc_w2_q[h * out_dim + o];
                logit += a1[h] * ((float)wq * s2);
            }
            if (o == 0 || logit > best) {
                best = logit;
                pred = o;
            }
        }
        if (pred == (int)mnist_fc_y[n])
            pass_fast++;
    }
    uint64_t t3 = timer_now();

    uint32_t us_block = ticks_to_us(t1 - t0);
    uint32_t us_fast = ticks_to_us(t3 - t2);
    uint32_t acc_block_x100 = (uint32_t)(pass_block * 10000 / samples);
    uint32_t acc_fast_x100 = (uint32_t)(pass_fast * 10000 / samples);

    xil_printf("  Samples: %d\n\r", samples);
    xil_printf("  Blocked path: %d/%d PASS, Acc=%d.%02d%%, Time=%d us, Avg=%d us\n\r",
               pass_block, samples, acc_block_x100 / 100, acc_block_x100 % 100,
               us_block, (samples > 0) ? (us_block / (uint32_t)samples) : 0);
    xil_printf("  Fast path:    %d/%d PASS, Acc=%d.%02d%%, Time=%d us, Avg=%d us\n\r",
               pass_fast, samples, acc_fast_x100 / 100, acc_fast_x100 % 100,
               us_fast, (samples > 0) ? (us_fast / (uint32_t)samples) : 0);
    if (us_fast > 0) {
        uint32_t speedup_x100 = (uint32_t)((uint64_t)us_block * 100ULL / (uint64_t)us_fast);
        xil_printf("  Speedup (blocked/fast): %d.%02d x\n\r", speedup_x100 / 100, speedup_x100 % 100);
    }

    Xil_DCacheDisable();
}
