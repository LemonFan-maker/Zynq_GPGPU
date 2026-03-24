#include "bench_common.h"
#include "bench_suite.h"

#define DP4A_N        64
#define DP4A_A_BASE   0
#define DP4A_B_BASE   DP4A_N
#define DP4A_O_BASE   (DP4A_N * 2)
#define DP4A_SUS_N     2048
#define DP4A_SUS_REPEAT 32
#define DP4A_SUS_A_BASE  0
#define DP4A_SUS_B_BASE  DP4A_SUS_N

static inline uint32_t enc_r(int opcode, int rd, int rs1, int rs2)
{
    return ((opcode & 0xF) << 28) |
           ((rd & 0x1F) << 23) |
           ((rs2 & 0x1F) << 13) |
           ((rs1 & 0x1F) << 8);
}

static inline uint32_t enc_i13(int opcode, int rd, int rs1, int imm13)
{
    const int hi5 = (imm13 >> 8) & 0x1F;
    const int lo8 = imm13 & 0xFF;
    return ((opcode & 0xF) << 28) |
           ((rd & 0x1F) << 23) |
           (hi5 << 13) |
           ((rs1 & 0x1F) << 8) |
           lo8;
}

static inline uint32_t enc_i8(int opcode, int rd, int rs1, int imm8)
{
    return ((opcode & 0xF) << 28) |
           ((rd & 0x1F) << 23) |
           ((rs1 & 0x1F) << 8) |
           (imm8 & 0xFF);
}

static inline uint32_t enc_bne(int rA, int rB, int offset13)
{
    const int hi5 = (offset13 >> 8) & 0x1F;
    const int lo8 = offset13 & 0xFF;
    return (0xEu << 28) |
           ((rB & 0x1F) << 23) |
           (hi5 << 13) |
           ((rA & 0x1F) << 8) |
           lo8;
}

static inline int32_t dp4a_ref(int32_t a, int32_t b, int32_t acc)
{
    int8_t a0 = (int8_t)((a >> 0) & 0xFF);
    int8_t a1 = (int8_t)((a >> 8) & 0xFF);
    int8_t a2 = (int8_t)((a >> 16) & 0xFF);
    int8_t a3 = (int8_t)((a >> 24) & 0xFF);
    int8_t b0 = (int8_t)((b >> 0) & 0xFF);
    int8_t b1 = (int8_t)((b >> 8) & 0xFF);
    int8_t b2 = (int8_t)((b >> 16) & 0xFF);
    int8_t b3 = (int8_t)((b >> 24) & 0xFF);
    return acc + (int32_t)a0 * b0 + (int32_t)a1 * b1 + (int32_t)a2 * b2 + (int32_t)a3 * b3;
}

static const int32_t g_dp4a_seed_a[8] = {
    0x01020304, 0x7F80FF01, 0x10111213, 0xFFEEDDCC,
    0x20212223, 0x80817F7E, 0x0A0B0C0D, 0xCAFEBABE
};

static const int32_t g_dp4a_seed_b[8] = {
    0x05060708, 0x01020304, 0x0A0B0C0D, 0x11223344,
    0x33445566, 0x7F7E7D7C, 0x01010101, 0x10203040
};

void bench_dp4a_smoke(void)
{
    xil_printf("\n\r========================================\n\r");
    xil_printf("  Benchmark 7: DP4A Microbenchmark (GPU real kernel)\n\r");
    xil_printf("========================================\n\r");

    for (int i = 0; i < DP4A_N; i++) {
        const uint32_t va = (uint32_t)g_dp4a_seed_a[i & 7];
        const uint32_t vb = (uint32_t)g_dp4a_seed_b[(i * 3) & 7];
        for (int l = 0; l < NUM_LANES; l++) {
            Xil_Out32(DDR_BUF_A + i * 32 + l * 4, va);
            Xil_Out32(DDR_BUF_A + (DP4A_B_BASE + i) * 32 + l * 4, vb);
        }
    }

    uint64_t t_load0 = timer_now();
    dma_to_dmem(DDR_BUF_A, DP4A_A_BASE, DP4A_N * 2);
    uint64_t t_load1 = timer_now();

    uint32_t dp4a_prog[] = {
        enc_i13(0xC, 5, 0, DP4A_N),      // r5 = N
        enc_i13(0xC, 4, 0, 0),           // r4 = i
        enc_i8 (0x8, 1, 4, 0),           // loop: r1 = A[i]
        enc_i13(0xC, 6, 4, DP4A_B_BASE), //       r6 = i + B_BASE
        enc_i8 (0x8, 2, 6, 0),           //       r2 = B[i]
        enc_r  (0x2, 3, 1, 2) | 0x80,    //       r3 = DP4A(r1, r2)
        enc_i13(0xC, 7, 4, DP4A_O_BASE), //       r7 = i + O_BASE
        ((0x9u << 28) | (3u << 13) | (7u << 8)), // STR r3, [r7+0]
        enc_i13(0xC, 4, 4, 1),           //       i++
        enc_bne(4, 5, -7),               //       if (i!=N) goto loop
        0x00000001                        // HALT
    };

    gpu_upload(dp4a_prog, (int)(sizeof(dp4a_prog) / sizeof(dp4a_prog[0])));
    uint64_t t_gpu = gpu_run();

    uint64_t t_store0 = timer_now();
    dma_to_ddr(DP4A_O_BASE, DDR_BUF_B, DP4A_N);
    uint64_t t_store1 = timer_now();

    int pass = 0;
    int shown_fail = 0;
    for (int i = 0; i < DP4A_N; i++) {
        const int32_t a = g_dp4a_seed_a[i & 7];
        const int32_t b = g_dp4a_seed_b[(i * 3) & 7];
        const int32_t exp = dp4a_ref(a, b, 0);
        const int32_t got = (int32_t)Xil_In32(DDR_BUF_B + i * 32);
        if (got == exp)
            pass++;
        else if (shown_fail < 4) {
            shown_fail++;
            xil_printf("  FAIL[%d]: got %d, exp %d\n\r", i, got, exp);
        }
    }

    const uint32_t us_load = ticks_to_us(t_load1 - t_load0);
    const uint32_t us_gpu = ticks_to_us(t_gpu);
    const uint32_t us_store = ticks_to_us(t_store1 - t_store0);
    const uint32_t us_total = us_load + us_gpu + us_store;
    const uint32_t int8_macs = DP4A_N * 4 * NUM_LANES;

    xil_printf("  Verify: %d/%d PASS\n\r", pass, DP4A_N);
    xil_printf("  DMA load  (%d entries): %d us\n\r", DP4A_N * 2, us_load);
    xil_printf("  GPU compute (%d instr): %d us\n\r", (int)(sizeof(dp4a_prog) / sizeof(dp4a_prog[0])), us_gpu);
    xil_printf("  DMA store (%d entries): %d us\n\r", DP4A_N, us_store);
    xil_printf("  Total pipeline:         %d us\n\r", us_total);
    xil_printf("  Effective INT8 MACs: %d (N=%d, 4 MAC/DP4A, %d lanes)\n\r", int8_macs, DP4A_N, NUM_LANES);
    if (us_gpu > 0)
        xil_printf("  GPU IMAC/s: %d\n\r", int8_macs / us_gpu);
}

void bench_dp4a_accumulator(void)
{
    xil_printf("\n\r========================================\n\r");
    xil_printf("  Benchmark 8: DP4A_ACC + ACC_NEXT (Accumulator real path)\n\r");
    xil_printf("========================================\n\r");

    for (int i = 0; i < DP4A_N; i++) {
        const uint32_t va = (uint32_t)g_dp4a_seed_a[(i * 5) & 7];
        const uint32_t vb = (uint32_t)g_dp4a_seed_b[(i * 3) & 7];
        for (int l = 0; l < NUM_LANES; l++) {
            Xil_Out32(DDR_BUF_A + i * 32 + l * 4, va);
            Xil_Out32(DDR_BUF_A + (DP4A_B_BASE + i) * 32 + l * 4, vb);
        }
    }

    uint64_t t_load0 = timer_now();
    dma_to_dmem(DDR_BUF_A, DP4A_A_BASE, DP4A_N * 2);
    uint64_t t_load1 = timer_now();

    uint32_t acc_prog[] = {
        enc_i13(0xC, 5, 0, DP4A_N),      // r5 = N(64)
        enc_i13(0xC, 4, 0, 0),           // r4 = i
        enc_i8 (0x8, 1, 4, 0),           // loop: r1 = A[i]
        enc_i13(0xC, 6, 4, DP4A_B_BASE), //       r6 = i + B_BASE
        enc_i8 (0x8, 2, 6, 0),           //       r2 = B[i]
        enc_r  (0x2, 0, 1, 2) | 0x82,    //       DP4A_ACC r1,r2  (acc += dot4)
        enc_r  (0x2, 0, 0, 0) | 0x03,    //       ACC_NEXT         (ptr++)
        enc_i13(0xC, 4, 4, 1),           //       i++
        enc_bne(4, 5, -6),               //       if (i!=N) goto loop
        0x00000001                        // HALT
    };

    gpu_upload(acc_prog, (int)(sizeof(acc_prog) / sizeof(acc_prog[0])));

    uint64_t t_gpu1 = gpu_run();
    uint64_t t_flush10 = timer_now();
    dma_acc_flush(DDR_BUF_B, DP4A_N);    // snapshot S1
    uint64_t t_flush11 = timer_now();

    uint64_t t_gpu2 = gpu_run();
    uint64_t t_flush20 = timer_now();
    dma_acc_flush(DDR_STAGE, DP4A_N);    // snapshot S2
    uint64_t t_flush21 = timer_now();

    int pass = 0;
    int shown_fail = 0;
    for (int i = 0; i < DP4A_N; i++) {
        const int32_t a = g_dp4a_seed_a[(i * 5) & 7];
        const int32_t b = g_dp4a_seed_b[(i * 3) & 7];
        const int32_t delta_exp = dp4a_ref(a, b, 0);

        const int32_t s1 = (int32_t)Xil_In32(DDR_BUF_B + i * 32);
        const int32_t s2 = (int32_t)Xil_In32(DDR_STAGE + i * 32);
        const int32_t delta_got = s2 - s1;

        if (delta_got == delta_exp) {
            pass++;
        } else if (shown_fail < 4) {
            shown_fail++;
            xil_printf("  FAIL[%d]: delta_got %d, delta_exp %d (s1=%d, s2=%d)\n\r",
                       i, delta_got, delta_exp, s1, s2);
        }
    }

    const uint32_t us_load = ticks_to_us(t_load1 - t_load0);
    const uint32_t us_gpu1 = ticks_to_us(t_gpu1);
    const uint32_t us_gpu2 = ticks_to_us(t_gpu2);
    const uint32_t us_flush1 = ticks_to_us(t_flush11 - t_flush10);
    const uint32_t us_flush2 = ticks_to_us(t_flush21 - t_flush20);
    const uint32_t us_total = us_load + us_gpu1 + us_flush1 + us_gpu2 + us_flush2;
    const uint32_t int8_macs_total = DP4A_N * 4 * NUM_LANES * 2;

    xil_printf("  Verify (delta check): %d/%d PASS\n\r", pass, DP4A_N);
    xil_printf("  DMA load (A+B, %d entries): %d us\n\r", DP4A_N * 2, us_load);
    xil_printf("  GPU pass#1 (%d instr): %d us\n\r", (int)(sizeof(acc_prog) / sizeof(acc_prog[0])), us_gpu1);
    xil_printf("  ACC flush S1 (%d entries): %d us\n\r", DP4A_N, us_flush1);
    xil_printf("  GPU pass#2 (%d instr): %d us\n\r", (int)(sizeof(acc_prog) / sizeof(acc_prog[0])), us_gpu2);
    xil_printf("  ACC flush S2 (%d entries): %d us\n\r", DP4A_N, us_flush2);
    xil_printf("  Total pipeline: %d us\n\r", us_total);
    xil_printf("  Effective INT8 MACs (2 passes): %d\n\r", int8_macs_total);
    if ((us_gpu1 + us_gpu2) > 0)
        xil_printf("  GPU IMAC/s (compute-only): %d\n\r", int8_macs_total / (us_gpu1 + us_gpu2));
}

void bench_dp4a_sustained(void)
{
    xil_printf("\n\r========================================\n\r");
    xil_printf("  Benchmark 9: DP4A Sustained Throughput\n\r");
    xil_printf("========================================\n\r");

    if ((DP4A_SUS_N & 7) != 0) {
        xil_printf("  [CFG-ERR] DP4A_SUS_N must be multiple of 8 for unrolled kernel.\n\r");
        return;
    }

    for (int i = 0; i < DP4A_SUS_N; i++) {
        const uint32_t va = (uint32_t)g_dp4a_seed_a[i & 7];
        const uint32_t vb = (uint32_t)g_dp4a_seed_b[(i * 3) & 7];
        for (int l = 0; l < NUM_LANES; l++) {
            Xil_Out32(DDR_BUF_A + i * 32 + l * 4, va);
            Xil_Out32(DDR_BUF_A + (DP4A_SUS_B_BASE + i) * 32 + l * 4, vb);
        }
    }

    uint64_t t_load0 = timer_now();
    dma_to_dmem(DDR_BUF_A, DP4A_SUS_A_BASE, DP4A_SUS_N * 2);
    uint64_t t_load1 = timer_now();

    uint32_t sus_prog[] = {
        enc_i13(0xC, 5, 0, DP4A_SUS_N),       // r5 = N
        enc_i13(0xC, 8, 0, DP4A_SUS_REPEAT),  // r8 = repeat
        enc_i13(0xC, 9, 0, 0),                // r9 = rep_idx
        enc_i13(0xC, 4, 0, 0),                // outer: r4 = i
        enc_i13(0xC, 6, 4, DP4A_SUS_B_BASE),  // inner: r6 = i + B_BASE
        enc_i8 (0x8,  1, 4, 0),               // A[i+0] -> r1
        enc_i8 (0x8,  2, 6, 0),               // B[i+0] -> r2
        enc_i8 (0x8, 10, 4, 1),               // A[i+1] -> r10
        enc_i8 (0x8, 11, 6, 1),               // B[i+1] -> r11
        enc_r  (0x2,  3, 1, 2)  | 0x80,       // DP4A #0 (r1,r2)
        enc_i8 (0x8,  1, 4, 2),               // A[i+2] -> r1
        enc_i8 (0x8,  2, 6, 2),               // B[i+2] -> r2
        enc_r  (0x2,  3,10,11) | 0x80,        // DP4A #1 (r10,r11)
        enc_i8 (0x8, 10, 4, 3),               // A[i+3] -> r10
        enc_i8 (0x8, 11, 6, 3),               // B[i+3] -> r11
        enc_r  (0x2,  3, 1, 2)  | 0x80,       // DP4A #2
        enc_i8 (0x8,  1, 4, 4),               // A[i+4] -> r1
        enc_i8 (0x8,  2, 6, 4),               // B[i+4] -> r2
        enc_r  (0x2,  3,10,11) | 0x80,        // DP4A #3
        enc_i8 (0x8, 10, 4, 5),               // A[i+5] -> r10
        enc_i8 (0x8, 11, 6, 5),               // B[i+5] -> r11
        enc_r  (0x2,  3, 1, 2)  | 0x80,       // DP4A #4
        enc_i8 (0x8,  1, 4, 6),               // A[i+6] -> r1
        enc_i8 (0x8,  2, 6, 6),               // B[i+6] -> r2
        enc_r  (0x2,  3,10,11) | 0x80,        // DP4A #5
        enc_i8 (0x8, 10, 4, 7),               // A[i+7] -> r10
        enc_i8 (0x8, 11, 6, 7),               // B[i+7] -> r11
        enc_r  (0x2,  3, 1, 2)  | 0x80,       // DP4A #6
        enc_r  (0x2,  3,10,11) | 0x80,        // DP4A #7
        enc_i13(0xC, 4, 4, 8),                // i += 8
        enc_bne(4, 5, -26),                   // if (i!=N) goto inner
        enc_i13(0xC, 9, 9, 1),                // rep_idx++
        enc_bne(9, 8, -29),                   // if (rep_idx!=repeat) goto outer
        0x00000001                            // HALT
    };

    gpu_upload(sus_prog, (int)(sizeof(sus_prog) / sizeof(sus_prog[0])));
    uint64_t t_gpu = gpu_run();

    const uint32_t us_load = ticks_to_us(t_load1 - t_load0);
    const uint32_t us_gpu = ticks_to_us(t_gpu);
    const uint32_t us_total = us_load + us_gpu;
    const uint32_t int8_macs = DP4A_SUS_N * DP4A_SUS_REPEAT * 4 * NUM_LANES;

    xil_printf("  Workload: N=%d, repeat=%d\n\r", DP4A_SUS_N, DP4A_SUS_REPEAT);
    xil_printf("  DMA load  (%d entries): %d us\n\r", DP4A_SUS_N * 2, us_load);
    xil_printf("  GPU compute (%d instr): %d us\n\r", (int)(sizeof(sus_prog) / sizeof(sus_prog[0])), us_gpu);
    xil_printf("  Total pipeline:         %d us\n\r", us_total);
    xil_printf("  Effective INT8 MACs: %d\n\r", int8_macs);
    if (us_gpu > 0)
        xil_printf("  GPU IMAC/s (compute-only): %d\n\r", int8_macs / us_gpu);
    if (us_total > 0)
        xil_printf("  GPU IMAC/s (end-to-end):   %d\n\r", int8_macs / us_total);
}
