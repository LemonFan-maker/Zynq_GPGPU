#include "bench_common.h"
#include "bench_suite.h"

#include "bench_vecadd.h"
#include "bench_matmul.h"
#include "conv2d_3x3.h"

void bench_dma(void)
{
    xil_printf("\n\r========================================\n\r");
    xil_printf("  Benchmark 1: DMA Bandwidth\n\r");
    xil_printf("========================================\n\r");

    uint32_t sizes[] = {16, 64, 256, 1024, 4095};
    int nsizes = sizeof(sizes) / sizeof(sizes[0]);

    for (uint32_t i = 0; i < 4096; i++)
        for (int l = 0; l < NUM_LANES; l++)
            Xil_Out32(DDR_BUF_A + i * 32 + l * 4, i);

    xil_printf("  entries | DDR->DMEM  | DMEM->DDR  |  bytes  | BW_in MB/s | BW_out MB/s\n\r");
    xil_printf("  --------|------------|------------|---------|------------|------------\n\r");

    for (int s = 0; s < nsizes; s++) {
        uint32_t n = sizes[s];
        uint32_t bytes = n * 32;
        uint64_t t0, t1, dt_in, dt_out;

        t0 = timer_now();
        dma_to_dmem(DDR_BUF_A, 0, n);
        t1 = timer_now();
        dt_in = t1 - t0;

        t0 = timer_now();
        dma_to_ddr(0, DDR_BUF_B, n);
        t1 = timer_now();
        dt_out = t1 - t0;

        uint32_t us_in  = ticks_to_us(dt_in);
        uint32_t us_out = ticks_to_us(dt_out);
        if (us_in == 0) us_in = 1;
        if (us_out == 0) us_out = 1;

        uint32_t bw_in  = bytes / us_in;
        uint32_t bw_out = bytes / us_out;

        xil_printf("  %4d    | %6d us  | %6d us  | %6d  |    %3d     |    %3d\n\r",
                   n, us_in, us_out, bytes, bw_in, bw_out);
    }

    xil_printf("\n\r  --- AXI-Lite comparison (256 entries) ---\n\r");
    uint64_t t0 = timer_now();
    for (uint32_t i = 0; i < 256; i++)
        for (int l = 0; l < NUM_LANES; l++)
            DMEM_WR(i, l, i);
    uint64_t t1 = timer_now();
    uint32_t us_axilite = ticks_to_us(t1 - t0);
    if (us_axilite == 0) us_axilite = 1;
    uint32_t bw_axilite = (256 * 32) / us_axilite;

    t0 = timer_now();
    dma_to_dmem(DDR_BUF_A, 0, 256);
    t1 = timer_now();
    uint32_t us_dma256 = ticks_to_us(t1 - t0);
    if (us_dma256 == 0) us_dma256 = 1;

    xil_printf("  AXI-Lite write 256 entries: %d us (%d MB/s)\n\r", us_axilite, bw_axilite);
    xil_printf("  DMA      write 256 entries: %d us (%d MB/s)\n\r", us_dma256, (256*32)/us_dma256);
    xil_printf("  Speedup: %dx\n\r", us_axilite / us_dma256);
}

void bench_vecadd_run(void)
{
    xil_printf("\n\r========================================\n\r");
    xil_printf("  Benchmark 2: Vector Add (256 entries)\n\r");
    xil_printf("========================================\n\r");

    for (uint32_t i = 0; i < 256; i++) {
        for (int l = 0; l < NUM_LANES; l++) {
            Xil_Out32(DDR_BUF_A + i * 32 + l * 4, i + 1);
            Xil_Out32(DDR_BUF_A + (256 + i) * 32 + l * 4, 1000);
        }
    }

    uint64_t t_load_start = timer_now();
    dma_to_dmem(DDR_BUF_A, 0, 512);
    uint64_t t_load_end = timer_now();

    gpu_upload(bench_vecadd, bench_vecadd_LEN);
    uint64_t t_gpu = gpu_run();

    uint64_t t_store_start = timer_now();
    dma_to_ddr(512, DDR_BUF_B, 256);
    uint64_t t_store_end = timer_now();

    int pass = 0, vfail = 0;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t got = Xil_In32(DDR_BUF_B + i * 32);
        if (got == i + 1 + 1000) {
            pass++;
        } else if (++vfail <= 3) {
            xil_printf("  FAIL[%d]: got %d, exp %d\n\r", i, got, i+1+1000);
        }
    }

    uint32_t us_load  = ticks_to_us(t_load_end - t_load_start);
    uint32_t us_gpu   = ticks_to_us(t_gpu);
    uint32_t us_store = ticks_to_us(t_store_end - t_store_start);
    uint32_t us_total = us_load + us_gpu + us_store;

    xil_printf("  Verify: %d/256 PASS\n\r", pass);
    xil_printf("  DMA load  (512 entries): %d us\n\r", us_load);
    xil_printf("  GPU compute:             %d us\n\r", us_gpu);
    xil_printf("  DMA store (256 entries): %d us\n\r", us_store);
    xil_printf("  Total pipeline:          %d us\n\r", us_total);
    xil_printf("  GPU throughput: 256 vec-adds x %d lanes = %d ops\n\r", NUM_LANES, 256 * NUM_LANES);
    if (us_gpu > 0)
        xil_printf("  GPU MOPS: %d\n\r", (256 * NUM_LANES) / us_gpu);
}

void bench_conv2d(void)
{
    xil_printf("\n\r========================================\n\r");
    xil_printf("  Benchmark 3: Conv2D 3x3 on 5x5\n\r");
    xil_printf("========================================\n\r");

    static const uint32_t expected[9] = {
        411, 456, 501, 636, 681, 726, 861, 906, 951
    };

    for (int i = 0; i < 25; i++)
        for (int l = 0; l < NUM_LANES; l++)
            Xil_Out32(DDR_BUF_A + i * 32 + l * 4, i + 1);

    uint64_t t_load_start = timer_now();
    dma_to_dmem(DDR_BUF_A, 0, 25);
    uint64_t t_load_end = timer_now();

    for (int i = 50; i < 59; i++)
        for (int l = 0; l < NUM_LANES; l++)
            DMEM_WR(i, l, 0);

    gpu_upload(conv2d_3x3, conv2d_3x3_LEN);
    uint64_t t_gpu = gpu_run();

    uint64_t t_store_start = timer_now();
    dma_to_ddr(50, DDR_BUF_B, 9);
    uint64_t t_store_end = timer_now();

    int pass = 0;
    for (int i = 0; i < 9; i++) {
        uint32_t got = Xil_In32(DDR_BUF_B + i * 32);
        if (got == expected[i])
            pass++;
        else
            xil_printf("  FAIL[%d]: got %d, exp %d\n\r", i, got, expected[i]);
    }

    uint32_t us_load  = ticks_to_us(t_load_end - t_load_start);
    uint32_t us_gpu   = ticks_to_us(t_gpu);
    uint32_t us_store = ticks_to_us(t_store_end - t_store_start);
    uint32_t us_total = us_load + us_gpu + us_store;

    uint32_t mac_ops = 9 * 9 * NUM_LANES;

    xil_printf("  Verify: %d/9 PASS\n\r", pass);
    xil_printf("  DMA load  (25 entries): %d us\n\r", us_load);
    xil_printf("  GPU compute (%d instr): %d us\n\r", conv2d_3x3_LEN, us_gpu);
    xil_printf("  DMA store  (9 entries): %d us\n\r", us_store);
    xil_printf("  Total pipeline:         %d us\n\r", us_total);
    xil_printf("  Effective MACs: %d (9 output x 9 kernel x %d lanes)\n\r", mac_ops, NUM_LANES);
}

void bench_matmul_run(void)
{
    xil_printf("\n\r========================================\n\r");
    xil_printf("  Benchmark 4: MatMul 8x8\n\r");
    xil_printf("========================================\n\r");

    for (int i = 0; i < 64; i++) {
        for (int l = 0; l < NUM_LANES; l++) {
            Xil_Out32(DDR_BUF_A + i * 32 + l * 4, i + 1);
            Xil_Out32(DDR_BUF_A + (64 + i) * 32 + l * 4, i + 1);
        }
    }

    uint64_t t_load_start = timer_now();
    dma_to_dmem(DDR_BUF_A, 0, 128);
    uint64_t t_load_end = timer_now();

    gpu_upload(bench_matmul, bench_matmul_LEN);
    uint64_t t_gpu = gpu_run();

    uint64_t t_store_start = timer_now();
    dma_to_ddr(128, DDR_BUF_B, 64);
    uint64_t t_store_end = timer_now();

    int pass = 0, fail = 0;
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            uint32_t acc = 0;
            for (int k = 0; k < 8; k++)
                acc += (uint32_t)(i * 8 + k + 1) * (uint32_t)(k * 8 + j + 1);
            uint32_t got = Xil_In32(DDR_BUF_B + (i * 8 + j) * 32);
            if (got == acc) {
                pass++;
            } else if (++fail <= 3) {
                xil_printf("  FAIL C[%d][%d]: got %d, exp %d\n\r", i, j, got, acc);
            }
        }
    }

    uint32_t us_load  = ticks_to_us(t_load_end - t_load_start);
    uint32_t us_gpu   = ticks_to_us(t_gpu);
    uint32_t us_store = ticks_to_us(t_store_end - t_store_start);
    uint32_t us_total = us_load + us_gpu + us_store;

    uint32_t mac_ops = 512 * NUM_LANES;

    xil_printf("  Verify: %d/64 PASS\n\r", pass);
    xil_printf("  DMA load  (128 entries): %d us\n\r", us_load);
    xil_printf("  GPU compute (%d instr): %d us\n\r", bench_matmul_LEN, us_gpu);
    xil_printf("  DMA store  (64 entries): %d us\n\r", us_store);
    xil_printf("  Total pipeline:          %d us\n\r", us_total);
    xil_printf("  MACs: %d (512 per matrix x %d lanes)\n\r", mac_ops, NUM_LANES);
    if (us_gpu > 0)
        xil_printf("  GPU MMAC/s: %d\n\r", mac_ops / us_gpu);
}

void bench_vdma_2d(void)
{
    xil_printf("\n\r========================================\n\r");
    xil_printf("  Benchmark VDMA: 2D Tile Extraction\n\r");
    xil_printf("========================================\n\r");

    uint32_t stride = 64 * 32;
    for (int r = 0; r < 64; r++) {
        for (int c = 0; c < 64; c++) {
            for (int l = 0; l < NUM_LANES; l++) {
                Xil_Out32(DDR_BUF_A + r * stride + c * 32 + l * 4, r * 1000 + c);
            }
        }
    }

    uint32_t start_addr = DDR_BUF_A + 10 * stride + 5 * 32;
    uint32_t x_items = 4;
    uint32_t y_items = 4;

    uint64_t t0 = timer_now();
    vdma_to_dmem(start_addr, 0, x_items, y_items, stride);
    uint64_t t1 = timer_now();

    int errors = 0;
    for (int r = 0; r < y_items; r++) {
        for (int c = 0; c < x_items; c++) {
            int dmem_idx = r * x_items + c;
            uint32_t expected = (10 + r) * 1000 + (5 + c);
            uint32_t actual = DMEM_RD(dmem_idx, 0);
            if (actual != expected) {
                errors++;
            }
        }
    }

    xil_printf("  VDMA 4x4 Tile Extraction from 64x64 matrix:\n\r");
    xil_printf("  Time: %d us\n\r", ticks_to_us(t1 - t0));
    if (errors == 0) {
        xil_printf("  Data verification: PASSED\n\r");
    } else {
        xil_printf("  Data verification: FAILED (%d errors)\n\r", errors);
    }
}
