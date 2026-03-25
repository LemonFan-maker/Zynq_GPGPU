#include "bench_common.h"
#include "bench_suite.h"

#include "mnist_fc_data.h"
#include "xil_cache.h"
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

#define MNIST_A1_Q_SHIFT 20
#define MNIST_A1_Q_MUL ((int32_t)(MNIST_FC_X_SCALE * MNIST_FC_W1_SCALE * 127.0f * (float)(1U << MNIST_A1_Q_SHIFT) + 0.5f))

static inline void fc1_blocked_accumulate(
    const uint8_t *x_row,
    int32_t *acc_h,
    int in_dim,
    int hid_dim,
    int tile,
    int w1_blocks_per_row
)
{
    const int rb_cnt = in_dim / tile;
    const int block_elems = tile * tile;
    for (int rb = 0; rb < rb_cnt; rb++) {
        const uint8_t *x_blk = x_row + rb * tile;
        for (int cb = 0; cb < w1_blocks_per_row; cb++) {
            const int16_t *blk = &mnist_fc_w1_blocks[(rb * w1_blocks_per_row + cb) * block_elems];
            const int h_base = cb * tile;
            for (int ri = 0; ri < tile; ri++) {
                const int32_t xi = (int32_t)x_blk[ri];
                const int16_t *w_row = blk + ri * tile;
                for (int ci = 0; ci < tile; ci++) {
                    acc_h[h_base + ci] += xi * (int32_t)w_row[ci];
                }
            }
        }
    }
}

static inline void fc1_fast_accumulate(const uint8_t *x_row, int32_t *acc_h, int in_dim, int hid_dim)
{
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
    int h = 0;
    for (; h + 15 < hid_dim; h += 16) {
        int32x4_t acc0 = vld1q_s32(&acc_h[h + 0]);
        int32x4_t acc1 = vld1q_s32(&acc_h[h + 4]);
        int32x4_t acc2 = vld1q_s32(&acc_h[h + 8]);
        int32x4_t acc3 = vld1q_s32(&acc_h[h + 12]);

        const int8_t *w_ptr = &mnist_fc_w1_q[h];
        for (int i = 0; i < in_dim; i++) {
            const int16x4_t x4 = vdup_n_s16((int16_t)x_row[i]);
            const int8x16_t w8 = vld1q_s8(w_ptr);
            const int16x8_t w_lo = vmovl_s8(vget_low_s8(w8));
            const int16x8_t w_hi = vmovl_s8(vget_high_s8(w8));
            acc0 = vmlal_s16(acc0, vget_low_s16(w_lo), x4);
            acc1 = vmlal_s16(acc1, vget_high_s16(w_lo), x4);
            acc2 = vmlal_s16(acc2, vget_low_s16(w_hi), x4);
            acc3 = vmlal_s16(acc3, vget_high_s16(w_hi), x4);
            w_ptr += hid_dim;
        }

        vst1q_s32(&acc_h[h + 0], acc0);
        vst1q_s32(&acc_h[h + 4], acc1);
        vst1q_s32(&acc_h[h + 8], acc2);
        vst1q_s32(&acc_h[h + 12], acc3);
    }

    for (; h < hid_dim; h++) {
        const int8_t *w_ptr = &mnist_fc_w1_q[h];
        int32_t acc = acc_h[h];
        for (int i = 0; i < in_dim; i++) {
            acc += (int32_t)x_row[i] * (int32_t)(*w_ptr);
            w_ptr += hid_dim;
        }
        acc_h[h] = acc;
    }
#else
    for (int i = 0; i < in_dim; i++) {
        const int32_t xi = (int32_t)x_row[i];
        const int8_t *w_row = &mnist_fc_w1_q[i * hid_dim];
        for (int h = 0; h < hid_dim; h++) {
            acc_h[h] += xi * (int32_t)w_row[h];
        }
    }
#endif
}

static inline void requant_relu_u8_127(const int32_t *acc_h, uint8_t *a1_q, int hid_dim, int32_t mul, int32_t round)
{
    for (int h = 0; h < hid_dim; h++) {
        int32_t acc = acc_h[h];
        if (acc < 0) acc = 0;
        int32_t q = (int32_t)(((int64_t)acc * (int64_t)mul + (int64_t)round) >> MNIST_A1_Q_SHIFT);
        if (q > 127) q = 127;
        a1_q[h] = (uint8_t)q;
    }
}

static inline void fc2_blocked_accumulate(
    const uint8_t *a1_q,
    int32_t *logits,
    int hid_dim,
    int out_dim,
    int tile,
    int w2_blocks_per_row
)
{
    const int rb_cnt = hid_dim / tile;
    const int block_elems = tile * tile;
    for (int rb = 0; rb < rb_cnt; rb++) {
        const uint8_t *a_blk = a1_q + rb * tile;
        for (int cb = 0; cb < w2_blocks_per_row; cb++) {
            const int16_t *blk = &mnist_fc_w2_blocks[(rb * w2_blocks_per_row + cb) * block_elems];
            const int o_base = cb * tile;
            const int o_valid = (o_base + tile <= out_dim) ? tile : (out_dim - o_base);
            for (int ri = 0; ri < tile; ri++) {
                const int32_t ai = (int32_t)a_blk[ri];
                const int16_t *w_row = blk + ri * tile;
                for (int ci = 0; ci < o_valid; ci++) {
                    logits[o_base + ci] += ai * (int32_t)w_row[ci];
                }
            }
        }
    }
}

static inline void fc2_fast_accumulate(const uint8_t *a1_q, int32_t *logits, int hid_dim, int out_dim)
{
    for (int h = 0; h < hid_dim; h++) {
        const int32_t ai = (int32_t)a1_q[h];
        const int8_t *w_row = &mnist_fc_w2_q[h * out_dim];
        for (int o = 0; o < out_dim; o++) {
            logits[o] += ai * (int32_t)w_row[o];
        }
    }
}

static inline int argmax_i32(const int32_t *x, int n)
{
    int best_i = 0;
    int32_t best_v = x[0];
    for (int i = 1; i < n; i++) {
        if (x[i] > best_v) {
            best_v = x[i];
            best_i = i;
        }
    }
    return best_i;
}

void bench_mnist_fc(void)
{
    xil_printf("\n\r========================================\n\r");
    xil_printf("  Benchmark 6: MNIST FC INT8 (ARM blocked vs fast)\n\r");
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

    const int32_t a1_q_mul = MNIST_A1_Q_MUL;
    const int32_t a1_q_round = (1 << (MNIST_A1_Q_SHIFT - 1));

    int pass_block = 0;
    int pass_fast = 0;
    uint8_t a1_q[MNIST_FC_HIDDEN_DIM];
    int32_t acc_h[MNIST_FC_HIDDEN_DIM];
    int32_t logits[MNIST_FC_OUT_DIM];

    uint64_t t0 = timer_now();
    for (int n = 0; n < samples; n++) {
        const uint8_t *x_row = &mnist_fc_x_q[n * in_dim];

        for (int h = 0; h < hid_dim; h++) {
            acc_h[h] = mnist_fc_b1_q[h];
        }
        fc1_blocked_accumulate(x_row, acc_h, in_dim, hid_dim, MNIST_FC_TILE, w1_blocks_per_row);
        requant_relu_u8_127(acc_h, a1_q, hid_dim, a1_q_mul, a1_q_round);

        for (int o = 0; o < out_dim; o++) {
            logits[o] = mnist_fc_b2_q[o];
        }
        fc2_blocked_accumulate(a1_q, logits, hid_dim, out_dim, MNIST_FC_TILE, w2_blocks_per_row);

        const int pred = argmax_i32(logits, out_dim);
        if (pred == (int)mnist_fc_y[n])
            pass_block++;
    }
    uint64_t t1 = timer_now();

    uint64_t t2 = timer_now();
    for (int n = 0; n < samples; n++) {
        const uint8_t *x_row = &mnist_fc_x_q[n * in_dim];

        for (int h = 0; h < hid_dim; h++) {
            acc_h[h] = mnist_fc_b1_q[h];
        }
        fc1_fast_accumulate(x_row, acc_h, in_dim, hid_dim);
        requant_relu_u8_127(acc_h, a1_q, hid_dim, a1_q_mul, a1_q_round);

        for (int o = 0; o < out_dim; o++) {
            logits[o] = mnist_fc_b2_q[o];
        }
        fc2_fast_accumulate(a1_q, logits, hid_dim, out_dim);

        const int pred = argmax_i32(logits, out_dim);
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
