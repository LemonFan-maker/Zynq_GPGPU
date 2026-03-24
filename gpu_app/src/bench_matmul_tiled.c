#include "bench_common.h"
#include "bench_suite.h"

#include "bench_matmul_pp.h"
#include "bench_matmul_pp_ovr.h"

#ifndef B5_DISABLE_OVERLAP
#define B5_DISABLE_OVERLAP 0
#endif

void bench_matmul_tiled_run(void)
{
    xil_printf("\n\r========================================\n\r");
    xil_printf("  Benchmark 5: Tiled MatMul 16x16 (HW Accumulator)\n\r");
    xil_printf("========================================\n\r");
#if B5_DISABLE_OVERLAP
    xil_printf("  [CFG] overlap preload: OFF (debug mode)\n\r");
#else
    xil_printf("  [CFG] overlap preload: ON\n\r");
#endif

    uint32_t stride = 16 * 32;
    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 16; c++) {
            uint32_t val_A = (uint32_t)(r * 16 + c + 1);
            uint32_t val_B = (uint32_t)(r * 16 + c + 1);
            for (int l = 0; l < NUM_LANES; l++) {
                Xil_Out32(DDR_BUF_A + r * stride + c * 32 + l * 4, val_A);
                Xil_Out32(DDR_BUF_B + r * stride + c * 32 + l * 4, val_B);
            }
        }
    }

    for (int i = 0; i < 256; i++)
        for (int l = 0; l < NUM_LANES; l++)
            Xil_Out32(DDR_C_OUT + i * 32 + l * 4, 0);

    gpu_upload(bench_matmul_pp, bench_matmul_pp_LEN);
    const uint32_t patch_pc_mul0 = 13;
    const uint32_t instr_mac_acc = bench_matmul_pp[patch_pc_mul0];
    const uint32_t instr_mul_ovr = bench_matmul_pp_ovr[patch_pc_mul0];
    const uint32_t base_by_tk[2] = {PING_BASE, PONG_BASE};

    uint64_t t_pipe_start = timer_now();
    uint64_t total_gpu_ticks = 0;

    for (int ti = 0; ti < 2; ti++) {
        for (int tj = 0; tj < 2; tj++) {
            uint32_t src_A = DDR_BUF_A + (ti * 8 * 16 + 0 * 8) * 32;
            uint32_t src_B = DDR_BUF_B + (0 * 8 * 16 + tj * 8) * 32;
            vdma_to_dmem(src_A, PING_BASE,      8, 8, stride);
            vdma_to_dmem(src_B, PING_BASE + 64, 8, 8, stride);

            for (int l = 0; l < NUM_LANES; l++)
                DMEM_WR(PARAM_ENTRY, l, base_by_tk[0]);
            gpu_patch_imem_word(patch_pc_mul0, instr_mul_ovr);

            uint64_t t0 = timer_now();
            uint32_t done_before = Xil_In32(GPU_DBG_DONES);
            gpu_stop();
            gpu_start();

#if !B5_DISABLE_OVERLAP
            {
                uint32_t src_A_next = DDR_BUF_A + (ti * 8 * 16 + 1 * 8) * 32;
                uint32_t src_B_next = DDR_BUF_B + (1 * 8 * 16 + tj * 8) * 32;
                vdma_to_dmem(src_A_next, base_by_tk[1],      8, 8, stride);
                vdma_to_dmem(src_B_next, base_by_tk[1] + 64, 8, 8, stride);
            }
#endif

            if (gpu_wait_done_edge_from(GPU_RUN_TIMEOUT_US, done_before) != 0) {
                xil_printf("  [GPU] timeout at tiled pass#1 (ti=%d,tj=%d)\n\r", ti, tj);
                xil_printf("  [GPU-DBG] pass#1 done_before=%u done_after=%u\n\r",
                           done_before, Xil_In32(GPU_DBG_DONES));
                gpu_debug_dump_state("tiled pass#1 timeout");
                gpu_stop();
                return;
            }
#if B5_DISABLE_OVERLAP
            {
                uint32_t src_A_next = DDR_BUF_A + (ti * 8 * 16 + 1 * 8) * 32;
                uint32_t src_B_next = DDR_BUF_B + (1 * 8 * 16 + tj * 8) * 32;
                vdma_to_dmem(src_A_next, base_by_tk[1],      8, 8, stride);
                vdma_to_dmem(src_B_next, base_by_tk[1] + 64, 8, 8, stride);
            }
#endif
            uint64_t t1 = timer_now();
            gpu_stop();
            total_gpu_ticks += (t1 - t0);

            for (int l = 0; l < NUM_LANES; l++)
                DMEM_WR(PARAM_ENTRY, l, base_by_tk[1]);
            gpu_patch_imem_word(patch_pc_mul0, instr_mac_acc);

            t0 = timer_now();
            done_before = Xil_In32(GPU_DBG_DONES);
            gpu_stop();
            gpu_start();
            if (gpu_wait_done_edge_from(GPU_RUN_TIMEOUT_US, done_before) != 0) {
                xil_printf("  [GPU] timeout at tiled pass#2 (ti=%d,tj=%d)\n\r", ti, tj);
                xil_printf("  [GPU-DBG] pass#2 done_before=%u done_after=%u\n\r",
                           done_before, Xil_In32(GPU_DBG_DONES));
                gpu_debug_dump_state("tiled pass#2 timeout");
                gpu_stop();
                return;
            }
            t1 = timer_now();
            gpu_stop();
            total_gpu_ticks += (t1 - t0);

            uint32_t c_dst = DDR_C_OUT + (ti * 8 * 16 + tj * 8) * 32;
            vdma_acc_flush(c_dst, 8, 8, stride);
        }
    }
    uint64_t t_pipe_end = timer_now();

    int pass = 0, fail_cnt = 0;
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            uint32_t acc = 0;
            for (int k = 0; k < 16; k++)
                acc += (uint32_t)(i * 16 + k + 1) * (uint32_t)(k * 16 + j + 1);
            uint32_t got = Xil_In32(DDR_C_OUT + (i * 16 + j) * 32);
            if (got == acc) {
                pass++;
            } else if (++fail_cnt <= 3) {
                xil_printf("  FAIL C[%d][%d]: got %d, exp %d\n\r", i, j, got, acc);
            }
        }
    }

    uint32_t us_pipe  = ticks_to_us(t_pipe_end - t_pipe_start);
    uint32_t us_gpu   = ticks_to_us(total_gpu_ticks);
    uint32_t mac_ops = 4096 * NUM_LANES;

    xil_printf("  Verify: %d/256 PASS\n\r", pass);
    xil_printf("  Pipeline (VDMA+GPU+acc_flush): %d us\n\r", us_pipe);
    xil_printf("    GPU compute (8 passes): %d us\n\r", us_gpu);
    xil_printf("  Total wall clock:         %d us\n\r", us_pipe);
    xil_printf("  MACs: %d (4096 per matrix x %d lanes)\n\r", mac_ops, NUM_LANES);
    if (us_pipe > 0)
        xil_printf("  Effective MMAC/s: %d\n\r", mac_ops / us_pipe);
}
