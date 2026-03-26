#include "bench_lenet.h"
#include "bench_suite.h"
#include "xil_printf.h"
#include <string.h>

#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#endif

// Conv2D
static void conv2d_hwc_cpu(
    const uint8_t *in, const int8_t *w, const int32_t *b, int32_t *out,
    int H, int W, int in_ch, int out_ch, int K
) {
    int out_H = H - K + 1;
    int out_W = W - K + 1;
    
    for (int y = 0; y < out_H; y++) {
        for (int x = 0; x < out_W; x++) {
            for (int oc = 0; out_ch - oc > 0; oc++) {
                int32_t sum = b[oc];
                for (int ky = 0; ky < K; ky++) {
                    for (int kx = 0; kx < K; kx++) {
                        for (int ic = 0; ic < in_ch; ic++) {
                            int in_y = y + ky;
                            int in_x = x + kx;
                            int in_val = in[(in_y * W + in_x) * in_ch + ic];
                            int w_val = w[((ky * K + kx) * in_ch + ic) * out_ch + oc];
                            sum += in_val * w_val;
                        }
                    }
                }
                out[(y * out_W + x) * out_ch + oc] = sum;
            }
        }
    }
}

// MaxPool2D
static void maxpool2d_hwc_cpu(
    const uint8_t *in, uint8_t *out,
    int H, int W, int ch, int K, int stride
) {
    int out_H = H / stride;
    int out_W = W / stride;
    
    for (int y = 0; y < out_H; y++) {
        for (int x = 0; x < out_W; x++) {
            for (int c = 0; c < ch; c++) {
                uint8_t max_val = 0;
                for (int ky = 0; ky < K; ky++) {
                    for (int kx = 0; kx < K; kx++) {
                        int in_y = y * stride + ky;
                        int in_x = x * stride + kx;
                        uint8_t val = in[(in_y * W + in_x) * ch + c];
                        if (val > max_val) max_val = val;
                    }
                }
                out[(y * out_W + x) * ch + c] = max_val;
            }
        }
    }
}

// FC
static void fc_cpu(
    const uint8_t *in, const int8_t *w, const int32_t *b, int32_t *out,
    int in_dim, int out_dim
) {
    for (int o = 0; o < out_dim; o++) {
        int32_t sum = b[o];
        for (int i = 0; i < in_dim; i++) {
            sum += in[i] * w[i * out_dim + o];
        }
        out[o] = sum;
    }
}

static void hwc_to_chw_u8(
    const uint8_t *in_hwc, uint8_t *out_chw,
    int H, int W, int C
) {
    for (int c = 0; c < C; c++) {
        for (int y = 0; y < H; y++) {
            for (int x = 0; x < W; x++) {
                out_chw[c * H * W + y * W + x] = in_hwc[(y * W + x) * C + c];
            }
        }
    }
}

#define FAST_OUT32(addr, val) (*(volatile uint32_t *)(addr) = (val))

static inline uint32_t enc_i13(int opcode, int rd, int rs1, int imm13) {
    return ((opcode & 0xF) << 28) | ((rd & 0x1F) << 23) | (((imm13 >> 8) & 0x1F) << 13) | ((rs1 & 0x1F) << 8) | (imm13 & 0xFF);
}
static inline uint32_t enc_i8(int opcode, int rd, int rs1, int imm8) {
    return ((opcode & 0xF) << 28) | ((rd & 0x1F) << 23) | ((rs1 & 0x1F) << 8) | (imm8 & 0xFF);
}
static inline uint32_t enc_r(int opcode, int rd, int rs1, int rs2) {
    return ((opcode & 0xF) << 28) | ((rd & 0x1F) << 23) | ((rs2 & 0x1F) << 13) | ((rs1 & 0x1F) << 8);
}
static inline uint32_t enc_str(int rs, int rb, int offset) {
    return (0x9u << 28) | ((rb & 0x1F) << 23) | (((offset >> 8) & 0x1F) << 13) | ((rs & 0x1F) << 8) | (offset & 0xFF);
}
static inline uint32_t enc_bne(int rA, int rB, int offset13) {
    return (0xEu << 28) | ((rB & 0x1F) << 23) | (((offset13 >> 8) & 0x1F) << 13) | ((rA & 0x1F) << 8) | (offset13 & 0xFF);
}

static inline uint32_t pack_i8x4(int8_t a0, int8_t a1, int8_t a2, int8_t a3) {
    return ((uint32_t)(uint8_t)a0) |
           ((uint32_t)(uint8_t)a1 << 8) |
           ((uint32_t)(uint8_t)a2 << 16) |
           ((uint32_t)(uint8_t)a3 << 24);
}

static inline void gpu_acc_clear_sync(void) {
    Xil_Out32(GPU_ACC_CLR, 1);
    Xil_Out32(GPU_ACC_CLR, 0);

    const uint64_t wait_ticks = (TIMER_FREQ_HZ / 1000000ULL) * 2ULL; // ~2us
    uint64_t t0 = timer_now();
    while ((timer_now() - t0) < wait_ticks) {
    }
}

#define FC1_IN_DIM 256
#define FC1_OUT_DIM 128
#define FC1_N_PACK (FC1_IN_DIM / 4)
#define FC1_OUT_BLK (FC1_OUT_DIM / 16)

#define FC2_IN_DIM 128
#define FC2_OUT_DIM 16
#define FC2_N_PACK (FC2_IN_DIM / 4)
#define FC2_OUT_BLK (FC2_OUT_DIM / 16)

#define CONV1_IN_H 28
#define CONV1_IN_W 28
#define CONV1_IN_CH 4
#define CONV1_K 5
#define CONV1_OUT_CH 16
#define CONV1_OUT_H (CONV1_IN_H - CONV1_K + 1)
#define CONV1_OUT_W (CONV1_IN_W - CONV1_K + 1)
#define CONV1_OUT_PIX (CONV1_OUT_H * CONV1_OUT_W)
#define CONV1_IN_DIM (CONV1_K * CONV1_K * CONV1_IN_CH)
#define CONV1_N_PACK (CONV1_IN_DIM / 4)
#define CONV1_TILE_PIX 64
#define CONV1_IN_MAX_ENTRIES (CONV1_TILE_PIX * CONV1_N_PACK)
#define CONV1_IN_BASE 32
#define CONV1_W_BASE 0

static uint32_t g_fc1_w_pack[FC1_OUT_BLK][FC1_N_PACK][NUM_LANES];
static uint32_t g_fc2_w_pack[FC2_OUT_BLK][FC2_N_PACK][NUM_LANES];
static uint32_t g_fc_in_pack_buf[FC1_N_PACK][NUM_LANES];
static uint32_t g_conv1_w_pack[CONV1_N_PACK][NUM_LANES];
static int g_fc_pack_inited = 0;
static int g_fc_ddr_inited = 0;
static int g_conv1_pack_inited = 0;
static int g_conv1_ddr_inited = 0;
static int g_conv1_im2col_offsets[CONV1_IN_DIM];
static int g_conv1_im2col_inited = 0;

#define LENET_FC1_W_DDR_BASE (0x10400000)
#define LENET_FC2_W_DDR_BASE (0x10500000)
#define LENET_CONV1_W_DDR_BASE (0x10600000)

static void lenet_fc_pack_init_once(void) {
    if (g_fc_pack_inited) {
        return;
    }

    // FC1: [256][128] -> 8 output blocks * 64 packs * 16 lanes
    for (int ob = 0; ob < FC1_OUT_BLK; ob++) {
        int out_base = ob * 16;
        for (int p = 0; p < FC1_N_PACK; p++) {
            int base = p * 4;
            for (int lane = 0; lane < NUM_LANES; lane++) {
                int o = out_base + lane;
                int8_t w0 = lenet_fc1_w_q[(base + 0) * FC1_OUT_DIM + o];
                int8_t w1 = lenet_fc1_w_q[(base + 1) * FC1_OUT_DIM + o];
                int8_t w2 = lenet_fc1_w_q[(base + 2) * FC1_OUT_DIM + o];
                int8_t w3 = lenet_fc1_w_q[(base + 3) * FC1_OUT_DIM + o];
                g_fc1_w_pack[ob][p][lane] = pack_i8x4(w0, w1, w2, w3);
            }
        }
    }

    // FC2: [128][16] -> 1 output block * 32 packs * 16 lanes
    for (int ob = 0; ob < FC2_OUT_BLK; ob++) {
        int out_base = ob * 16;
        for (int p = 0; p < FC2_N_PACK; p++) {
            int base = p * 4;
            for (int lane = 0; lane < NUM_LANES; lane++) {
                int o = out_base + lane;
                int8_t w0 = lenet_fc2_w_q[(base + 0) * FC2_OUT_DIM + o];
                int8_t w1 = lenet_fc2_w_q[(base + 1) * FC2_OUT_DIM + o];
                int8_t w2 = lenet_fc2_w_q[(base + 2) * FC2_OUT_DIM + o];
                int8_t w3 = lenet_fc2_w_q[(base + 3) * FC2_OUT_DIM + o];
                g_fc2_w_pack[ob][p][lane] = pack_i8x4(w0, w1, w2, w3);
            }
        }
    }

    g_fc_pack_inited = 1;
}

static void lenet_fc_pack_to_ddr_once(void) {
    if (g_fc_ddr_inited) {
        return;
    }

    lenet_fc_pack_init_once();

    for (int ob = 0; ob < FC1_OUT_BLK; ob++) {
        for (int p = 0; p < FC1_N_PACK; p++) {
            uint32_t entry_addr = LENET_FC1_W_DDR_BASE + (ob * FC1_N_PACK + p) * BYTES_PER_ENTRY;
            volatile uint32_t *p_addr = (volatile uint32_t *)entry_addr;
            for (int lane = 0; lane < NUM_LANES; lane++) {
                p_addr[lane] = g_fc1_w_pack[ob][p][lane];
            }
        }
    }

    for (int ob = 0; ob < FC2_OUT_BLK; ob++) {
        for (int p = 0; p < FC2_N_PACK; p++) {
            uint32_t entry_addr = LENET_FC2_W_DDR_BASE + (ob * FC2_N_PACK + p) * BYTES_PER_ENTRY;
            volatile uint32_t *p_addr = (volatile uint32_t *)entry_addr;
            for (int lane = 0; lane < NUM_LANES; lane++) {
                p_addr[lane] = g_fc2_w_pack[ob][p][lane];
            }
        }
    }

    g_fc_ddr_inited = 1;
}

static void lenet_conv1_pack_init_once(void) {
    if (g_conv1_pack_inited) {
        return;
    }

    for (int p = 0; p < CONV1_N_PACK; p++) {
        int base = p * 4;
        for (int lane = 0; lane < NUM_LANES; lane++) {
            int8_t w0 = lenet_conv1_w_q[(base + 0) * CONV1_OUT_CH + lane];
            int8_t w1 = lenet_conv1_w_q[(base + 1) * CONV1_OUT_CH + lane];
            int8_t w2 = lenet_conv1_w_q[(base + 2) * CONV1_OUT_CH + lane];
            int8_t w3 = lenet_conv1_w_q[(base + 3) * CONV1_OUT_CH + lane];
            g_conv1_w_pack[p][lane] = pack_i8x4(w0, w1, w2, w3);
        }
    }

    g_conv1_pack_inited = 1;

    if (!g_conv1_im2col_inited) {
        for(int idx = 0; idx < CONV1_IN_DIM; idx++) {
            int ky = idx / (CONV1_K * CONV1_IN_CH);
            int rem = idx % (CONV1_K * CONV1_IN_CH);
            int kx = rem / CONV1_IN_CH;
            int ic = rem % CONV1_IN_CH;
            g_conv1_im2col_offsets[idx] = (ky * CONV1_IN_W + kx) * CONV1_IN_CH + ic;
        }
        g_conv1_im2col_inited = 1;
    }
}

static void lenet_conv1_pack_to_ddr_once(void) {
    if (g_conv1_ddr_inited) {
        return;
    }

    lenet_conv1_pack_init_once();

    for (int p = 0; p < CONV1_N_PACK; p++) {
        uint32_t entry_addr = LENET_CONV1_W_DDR_BASE + p * BYTES_PER_ENTRY;
        volatile uint32_t *p_addr = (volatile uint32_t *)entry_addr;
        for (int lane = 0; lane < NUM_LANES; lane++) {
            p_addr[lane] = g_conv1_w_pack[p][lane];
        }
    }

    g_conv1_ddr_inited = 1;
}

static void fc_gpu_dp4a_u8_i8(
    const uint8_t *in_u8,
    const int8_t *w_i8,
    const int32_t *b_i32,
    int in_dim,
    int out_dim,
    int32_t *out_i32
) {
    if ((in_dim & 3) != 0 || (out_dim & 15) != 0) {
        fc_cpu(in_u8, w_i8, b_i32, out_i32, in_dim, out_dim);
        return;
    }

    const int n_pack = in_dim / 4;
    const int out_blk = out_dim / 16;
    const int in_base = 0;
    const int w_base = n_pack;

    lenet_fc_pack_to_ddr_once();

    // Fixed micro-kernel.
    uint32_t prog[] = {
        enc_i13(0xC, 30, 0, 1),           // r30 = 1
        enc_i13(0xC, 31, 0, 1),           // r31 = 1
        enc_r(0xA, 0, 30, 31),            // SETM all lanes enabled
        enc_i13(0xC, 5, 0, n_pack),       // r5 = N
        enc_i13(0xC, 4, 0, 0),            // r4 = p
        enc_i13(0xC, 6, 4, in_base),      // loop: r6 = IN_BASE + p
        enc_i13(0xC, 7, 4, w_base),       //       r7 = W_BASE + p
        enc_i8(0x8, 1, 6, 0),             //       r1 = x_pack[p]
        enc_i8(0x8, 2, 7, 0),             //       r2 = w_pack[p]
        enc_r(0x2, 0, 1, 2) | 0x82,       //       DP4A_ACC(r1,r2)
        enc_i13(0xC, 4, 4, 1),            //       p++
        enc_bne(4, 5, -6),                //       if (p!=N) goto loop
        0x00000001                          // HALT
    };
    gpu_upload(prog, (int)(sizeof(prog) / sizeof(prog[0])));

    for (int p = 0; p < n_pack; p++) {
        int base = p * 4;
        uint32_t x_pack = ((uint32_t)in_u8[base + 0]) |
                          ((uint32_t)in_u8[base + 1] << 8) |
                          ((uint32_t)in_u8[base + 2] << 16) |
                          ((uint32_t)in_u8[base + 3] << 24);
        for (int l = 0; l < NUM_LANES; l++) {
            g_fc_in_pack_buf[p][l] = x_pack;
        }
    }

    for (int p = 0; p < n_pack; p++) {
        uint32_t entry_addr = DDR_BUF_A + p * BYTES_PER_ENTRY;
        volatile uint32_t *p_addr = (volatile uint32_t *)entry_addr;
        for (int lane = 0; lane < NUM_LANES; lane++) {
            p_addr[lane] = g_fc_in_pack_buf[p][lane];
        }
    }

    dma_to_dmem(DDR_BUF_A, in_base, n_pack);

    for (int ob = 0; ob < out_blk; ob++) {
        int out_base = ob * 16;

        if (w_i8 == lenet_fc1_w_q && in_dim == FC1_IN_DIM && out_dim == FC1_OUT_DIM) {
            uint32_t src = LENET_FC1_W_DDR_BASE + ob * FC1_N_PACK * BYTES_PER_ENTRY;
            dma_to_dmem(src, w_base, n_pack);
        } else if (w_i8 == lenet_fc2_w_q && in_dim == FC2_IN_DIM && out_dim == FC2_OUT_DIM) {
            uint32_t src = LENET_FC2_W_DDR_BASE + ob * FC2_N_PACK * BYTES_PER_ENTRY;
            dma_to_dmem(src, w_base, n_pack);
        } else {
            for (int p = 0; p < n_pack; p++) {
                int base = p * 4;
                for (int lane = 0; lane < 16; lane++) {
                    int o = out_base + lane;
                    int8_t ww0 = w_i8[(base + 0) * out_dim + o];
                    int8_t ww1 = w_i8[(base + 1) * out_dim + o];
                    int8_t ww2 = w_i8[(base + 2) * out_dim + o];
                    int8_t ww3 = w_i8[(base + 3) * out_dim + o];
                    g_fc_in_pack_buf[p][lane] = pack_i8x4(ww0, ww1, ww2, ww3);
                }
            }
            for (int p = 0; p < n_pack; p++) {
                uint32_t entry_addr = DDR_BUF_A + (n_pack + p) * BYTES_PER_ENTRY;
                volatile uint32_t *p_addr = (volatile uint32_t *)entry_addr;
                for (int lane = 0; lane < NUM_LANES; lane++) {
                    p_addr[lane] = g_fc_in_pack_buf[p][lane];
                }
            }
            dma_to_dmem(DDR_BUF_A + n_pack * BYTES_PER_ENTRY, w_base, n_pack);
        }

        gpu_acc_clear_sync();
        (void)gpu_run();
        dma_acc_flush(DDR_STAGE, 1);

        volatile uint32_t *p_out_addr = (volatile uint32_t *)(DDR_STAGE);
        for (int lane = 0; lane < 16; lane++) {
            int32_t dot = (int32_t)p_out_addr[lane];
            out_i32[out_base + lane] = dot + b_i32[out_base + lane];
        }
    }
}

static int gen_conv1_dp4a_tile_kernel(uint32_t *prog, int tile_pix) {
    int i = 0;
    prog[i++] = enc_i13(0xC, 30, 0, 1);            // r30 = 1
    prog[i++] = enc_i13(0xC, 31, 0, 1);            // r31 = 1
    prog[i++] = enc_r(0xA, 0, 30, 31);             // SETM all lanes enabled
    prog[i++] = enc_i13(0xC, 10, 0, tile_pix);     // r10 = tile_pix
    prog[i++] = enc_i13(0xC, 9, 0, 0);             // r9  = pix_idx
    prog[i++] = enc_i13(0xC, 11, 0, CONV1_IN_BASE);// r11 = in_ptr(base)
    prog[i++] = enc_i13(0xC, 5, 0, CONV1_N_PACK);  // r5  = N_PACK

    // pix_loop:
    prog[i++] = enc_i13(0xC, 4, 0, 0);             // r4 = p
    // p_loop:
    prog[i++] = enc_i13(0xC, 6, 4, 0);             // r6 = in_ptr + p
    prog[i++] = enc_r(0x0, 6, 6, 11);              // r6 = r6 + r11
    prog[i++] = enc_i13(0xC, 7, 4, CONV1_W_BASE);  // r7 = w_base + p
    prog[i++] = enc_i8(0x8, 1, 6, 0);              // r1 = x_pack
    prog[i++] = enc_i8(0x8, 2, 7, 0);              // r2 = w_pack
    prog[i++] = enc_r(0x2, 0, 1, 2) | 0x82;        // DP4A_ACC
    prog[i++] = enc_i13(0xC, 4, 4, 1);             // p++
    prog[i++] = enc_bne(4, 5, -7);                 // if (p!=N_PACK) goto p_loop

    prog[i++] = enc_r(0x2, 0, 0, 0) | 0x03;        // ACC_NEXT
    prog[i++] = enc_i13(0xC, 9, 9, 1);             // pix_idx++
    prog[i++] = enc_i13(0xC, 11, 11, CONV1_N_PACK);// in_ptr += N_PACK
    prog[i++] = enc_bne(9, 10, -12);               // if (pix_idx!=tile_pix) goto pix_loop
    prog[i++] = 0x00000001;                        // HALT
    return i;
}

static void conv1_gpu_dp4a_u8_i8(
    const uint8_t *in_u8,
    const int8_t *w_i8,
    const int32_t *b_i32,
    int32_t *out_i32
) {
    if (w_i8 != lenet_conv1_w_q) {
        conv2d_hwc_cpu(in_u8, w_i8, b_i32, out_i32, CONV1_IN_H, CONV1_IN_W, CONV1_IN_CH, CONV1_OUT_CH, CONV1_K);
        return;
    }

    lenet_conv1_pack_to_ddr_once();
    dma_to_dmem(LENET_CONV1_W_DDR_BASE, CONV1_W_BASE, CONV1_N_PACK);

    uint32_t prog[32];
    int prog_len = gen_conv1_dp4a_tile_kernel(prog, CONV1_TILE_PIX);
    gpu_upload(prog, prog_len);

    for (int tile_start = 0; tile_start < CONV1_OUT_PIX; tile_start += CONV1_TILE_PIX) {
        int tile_pix = CONV1_TILE_PIX;
        if (tile_start + tile_pix > CONV1_OUT_PIX) {
            tile_pix = CONV1_OUT_PIX - tile_start;
            prog_len = gen_conv1_dp4a_tile_kernel(prog, tile_pix);
            gpu_upload(prog, prog_len);
        }

        int in_entries = tile_pix * CONV1_N_PACK;
        for (int local = 0; local < tile_pix; local++) {
            int pix = tile_start + local;
            int y = pix / CONV1_OUT_W;
            int x = pix % CONV1_OUT_W;
            const uint8_t *in_base_ptr = in_u8 + (y * CONV1_IN_W + x) * CONV1_IN_CH;
            
            uint32_t entry_addr = DDR_BUF_A + (local * CONV1_N_PACK) * BYTES_PER_ENTRY;
            for (int p = 0; p < CONV1_N_PACK; p++) {
                int base = p * 4;
                uint32_t val0 = in_base_ptr[g_conv1_im2col_offsets[base + 0]];
                uint32_t val1 = in_base_ptr[g_conv1_im2col_offsets[base + 1]];
                uint32_t val2 = in_base_ptr[g_conv1_im2col_offsets[base + 2]];
                uint32_t val3 = in_base_ptr[g_conv1_im2col_offsets[base + 3]];
                uint32_t x_pack = val0 | (val1 << 8) | (val2 << 16) | (val3 << 24);

                volatile uint32_t *p_addr = (volatile uint32_t *)entry_addr;
                for (int lane = 0; lane < NUM_LANES; lane++) {
                    p_addr[lane] = x_pack;
                }
                entry_addr += BYTES_PER_ENTRY;
            }
        }
        
        dma_to_dmem(DDR_BUF_A, CONV1_IN_BASE, in_entries);

        gpu_acc_clear_sync();
        (void)gpu_run();
        dma_acc_flush(DDR_STAGE, tile_pix);

        for (int local = 0; local < tile_pix; local++) {
            int out_base = (tile_start + local) * CONV1_OUT_CH;
            uint32_t entry_addr = DDR_STAGE + local * BYTES_PER_ENTRY;
            volatile uint32_t *p_out_addr = (volatile uint32_t *)entry_addr;
            for (int lane = 0; lane < NUM_LANES; lane++) {
                int32_t dot = (int32_t)p_out_addr[lane];
                out_i32[out_base + lane] = dot + b_i32[lane];
            }
        }
    }
}

// Hybrid inference: only Conv1 on GPU, FC1/FC2 on CPU
static int lenet_cpu_infer(const uint8_t *img) {
    static int32_t c1_acc[16 * 24 * 24];
    static uint8_t c1_out[16 * 24 * 24];
    static uint8_t p1_out[16 * 12 * 12];
    static int32_t c2_acc[16 * 8 * 8];
    static uint8_t c2_out[16 * 8 * 8];
    static uint8_t p2_out[16 * 4 * 4];
    static uint8_t p2_chw[16 * 4 * 4];
    static int32_t fc1_acc[128];
    static uint8_t fc1_out[128];
    static int32_t fc2_acc[16];
    
    conv2d_hwc_cpu(img, lenet_conv1_w_q, lenet_conv1_b_q, c1_acc, 28, 28, 4, 16, 5);
    requantivate_u8_relu(c1_acc, c1_out, 16*24*24, LENET_X_SCALE, LENET_CONV1_W_SCALE, LENET_CONV1_OUT_SCALE);
    
    maxpool2d_hwc_cpu(c1_out, p1_out, 24, 24, 16, 2, 2);
    
    conv2d_hwc_cpu(p1_out, lenet_conv2_w_q, lenet_conv2_b_q, c2_acc, 12, 12, 16, 16, 5);
    requantivate_u8_relu(c2_acc, c2_out, 16*8*8, LENET_CONV1_OUT_SCALE, LENET_CONV2_W_SCALE, LENET_CONV2_OUT_SCALE);
    
    maxpool2d_hwc_cpu(c2_out, p2_out, 8, 8, 16, 2, 2);

    hwc_to_chw_u8(p2_out, p2_chw, 4, 4, 16);
    
    fc_cpu(p2_chw, lenet_fc1_w_q, lenet_fc1_b_q, fc1_acc, 256, 128);
    requantivate_u8_relu(fc1_acc, fc1_out, 128, LENET_CONV2_OUT_SCALE, LENET_FC1_W_SCALE, LENET_FC1_OUT_SCALE);
    
    fc_cpu(fc1_out, lenet_fc2_w_q, lenet_fc2_b_q, fc2_acc, 128, 16);
    
    int max_idx = 0;
    int32_t max_val = fc2_acc[0];
    for (int j = 1; j < 16; j++) {
        if (fc2_acc[j] > max_val) {
            max_val = fc2_acc[j];
            max_idx = j;
        }
    }
    return max_idx;
}

// Hybrid inference: conv1/fc1/fc2 on GPU, remaining layers on CPU
static int lenet_hybrid_infer_gpu_conv1_fc(const uint8_t *img) {
    static int32_t c1_acc[16 * 24 * 24];
    static uint8_t c1_out[16 * 24 * 24];
    static uint8_t p1_out[16 * 12 * 12];
    static int32_t c2_acc[16 * 8 * 8];
    static uint8_t c2_out[16 * 8 * 8];
    static uint8_t p2_out[16 * 4 * 4];
    static uint8_t p2_chw[16 * 4 * 4];
    static int32_t fc1_acc[128];
    static uint8_t fc1_out[128];
    static int32_t fc2_acc[16];

    conv1_gpu_dp4a_u8_i8(img, lenet_conv1_w_q, lenet_conv1_b_q, c1_acc);
    requantivate_u8_relu(c1_acc, c1_out, 16 * 24 * 24, LENET_X_SCALE, LENET_CONV1_W_SCALE, LENET_CONV1_OUT_SCALE);

    maxpool2d_hwc_cpu(c1_out, p1_out, 24, 24, 16, 2, 2);

    conv2d_hwc_cpu(p1_out, lenet_conv2_w_q, lenet_conv2_b_q, c2_acc, 12, 12, 16, 16, 5);
    requantivate_u8_relu(c2_acc, c2_out, 16 * 8 * 8, LENET_CONV1_OUT_SCALE, LENET_CONV2_W_SCALE, LENET_CONV2_OUT_SCALE);

    maxpool2d_hwc_cpu(c2_out, p2_out, 8, 8, 16, 2, 2);
    hwc_to_chw_u8(p2_out, p2_chw, 4, 4, 16);

    fc_gpu_dp4a_u8_i8(p2_chw, lenet_fc1_w_q, lenet_fc1_b_q, 256, 128, fc1_acc);
    requantivate_u8_relu(fc1_acc, fc1_out, 128, LENET_CONV2_OUT_SCALE, LENET_FC1_W_SCALE, LENET_FC1_OUT_SCALE);

    fc_gpu_dp4a_u8_i8(fc1_out, lenet_fc2_w_q, lenet_fc2_b_q, 128, 16, fc2_acc);

    int max_idx = 0;
    int32_t max_val = fc2_acc[0];
    for (int j = 1; j < 16; j++) {
        if (fc2_acc[j] > max_val) {
            max_val = fc2_acc[j];
            max_idx = j;
        }
    }
    return max_idx;
}

void bench_lenet_run() {

    xil_printf("\n\r========================================\n\r");
    xil_printf("  Benchmark 10: LeNet CPU Reference (Full INT8)\n\r");
    xil_printf("========================================\n\r");

    int correct = 0;
    int correct_hybrid = 0;
    int agree = 0;
    int total_samples = 32;
    int pred_cpu[32];
    int pred_hybrid[32];
    
    uint64_t t0 = timer_now();
    for (int i = 0; i < total_samples; i++) {
        const uint8_t *img = &lenet_x_q[i * 28 * 28 * 4];
        int pred = lenet_cpu_infer(img);
        int label = lenet_y[i];
        pred_cpu[i] = pred;
        
        if (pred == label) {
            correct++;
        } else {
            xil_printf("  Sample %d MISMATCH: Pred=%d, Label=%d\n\r", i, pred, label);
        }
    }
    uint64_t t1 = timer_now();

    uint64_t t2 = timer_now();
    for (int i = 0; i < total_samples; i++) {
        const uint8_t *img = &lenet_x_q[i * 28 * 28 * 4];
        int pred = lenet_hybrid_infer_gpu_conv1_fc(img);
        int label = lenet_y[i];
        pred_hybrid[i] = pred;

        if (pred == label) {
            correct_hybrid++;
        }

        if (pred == pred_cpu[i]) {
            agree++;
        } else {
            xil_printf("  Sample %d HYBRID DIFF: CPU=%d, GPU=%d, Label=%d\n\r", i, pred_cpu[i], pred, label);
        }
    }
    uint64_t t3 = timer_now();
    
    uint32_t dt_cpu_us = ticks_to_us(t1 - t0);
    uint32_t dt_hybrid_us = ticks_to_us(t3 - t2);
    uint32_t ms_cpu_per_sample = dt_cpu_us / total_samples / 1000;
    uint32_t ms_hybrid_per_sample = dt_hybrid_us / total_samples / 1000;
    
    uint32_t acc = correct * 100 / total_samples;
    uint32_t acc_hybrid = correct_hybrid * 100 / total_samples;
    uint32_t agree_ratio = agree * 100 / total_samples;
    xil_printf("  Test Accuracy: %d / %d (%d%%)\n\r", correct, total_samples, acc);
    xil_printf("  Hybrid Accuracy (GPU Conv1+FC1+FC2): %d / %d (%d%%)\n\r", correct_hybrid, total_samples, acc_hybrid);
    xil_printf("  CPU vs Hybrid Agreement: %d / %d (%d%%)\n\r", agree, total_samples, agree_ratio);
    xil_printf("  CPU Time: %d us (%d ms/sample)\n\r", dt_cpu_us, ms_cpu_per_sample);
    xil_printf("  Hybrid Time: %d us (%d ms/sample)\n\r", dt_hybrid_us, ms_hybrid_per_sample);
}
