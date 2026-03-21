#include "xil_printf.h"
#include "xil_io.h"
#include "xil_cache.h"
#include "conv2d_3x3.h"
#include "bench_vecadd.h"
#include "bench_matmul.h"
#include "bench_matmul_pp.h"
#include "bench_matmul_pp_ovr.h"
#include "mnist_fc_data.h"

#define GT_COUNTER_LO  0xF8F00200
#define GT_COUNTER_HI  0xF8F00204
#define GT_CONTROL     0xF8F00208
#define TIMER_FREQ_HZ  333333333ULL  

static inline void timer_init(void)
{
    
    Xil_Out32(GT_CONTROL, Xil_In32(GT_CONTROL) | 1);
}

static inline uint64_t timer_now(void)
{
    uint32_t hi, lo, hi2;
    do {
        hi  = Xil_In32(GT_COUNTER_HI);
        lo  = Xil_In32(GT_COUNTER_LO);
        hi2 = Xil_In32(GT_COUNTER_HI);
    } while (hi != hi2);  
    return ((uint64_t)hi << 32) | lo;
}

static inline uint32_t ticks_to_us(uint64_t dt)
{
    return (uint32_t)(dt / (TIMER_FREQ_HZ / 1000000));
}

#define GPU_BASE       0x40000000
#define GPU_CTRL       (GPU_BASE + 0x000)
#define GPU_STATUS     (GPU_BASE + 0x004)
#define GPU_DMA_SRC    (GPU_BASE + 0x008)
#define GPU_DMA_DST    (GPU_BASE + 0x00C)
#define GPU_DMA_X_SIZE (GPU_BASE + 0x010)
#define GPU_DMA_Y_SIZE (GPU_BASE + 0x014)
#define GPU_DMA_STRIDE (GPU_BASE + 0x018)
#define GPU_DMA_CMD    (GPU_BASE + 0x01C)
#define GPU_DMA_STATUS (GPU_BASE + 0x020)
#define GPU_ACC_CLR    (GPU_BASE + 0x024)
#define GPU_IMEM_BASE  (GPU_BASE + 0x100)
#define GPU_DMEM_BASE  (GPU_BASE + 0x2000)

#define NUM_LANES 8
#define DMEM_WR(e,l,v) Xil_Out32(GPU_DMEM_BASE + (e)*32 + (l)*4, (v))
#define DMEM_RD(e,l)   Xil_In32(GPU_DMEM_BASE + (e)*32 + (l)*4)

#define DDR_BUF_A  0x10000000
#define DDR_BUF_B  0x10100000
#define DDR_STAGE  0x10200000  
#define DDR_C_OUT  0x10300000  

#define PARAM_ENTRY 250  
#define PING_BASE   0
#define PONG_BASE   512

static void dma_to_dmem(uint32_t ddr, uint32_t entry, uint32_t n)
{
    Xil_Out32(GPU_DMA_SRC, ddr);
    Xil_Out32(GPU_DMA_DST, entry);
    Xil_Out32(GPU_DMA_X_SIZE, n);
    Xil_Out32(GPU_DMA_Y_SIZE, 1);
    Xil_Out32(GPU_DMA_STRIDE, n * 32);
    Xil_Out32(GPU_DMA_CMD, 0x01);
    while (Xil_In32(GPU_DMA_STATUS) & 1);
}

static void dma_to_ddr(uint32_t entry, uint32_t ddr, uint32_t n)
{
    Xil_Out32(GPU_DMA_SRC, entry);
    Xil_Out32(GPU_DMA_DST, ddr);
    Xil_Out32(GPU_DMA_X_SIZE, n);
    Xil_Out32(GPU_DMA_Y_SIZE, 1);
    Xil_Out32(GPU_DMA_STRIDE, n * 32);
    Xil_Out32(GPU_DMA_CMD, 0x03);
    while (Xil_In32(GPU_DMA_STATUS) & 1);
}

static void vdma_to_dmem(uint32_t ddr, uint32_t entry, uint32_t x_size, uint32_t y_size, uint32_t stride)
{
    Xil_Out32(GPU_DMA_SRC, ddr);
    Xil_Out32(GPU_DMA_DST, entry);
    Xil_Out32(GPU_DMA_X_SIZE, x_size);
    Xil_Out32(GPU_DMA_Y_SIZE, y_size);
    Xil_Out32(GPU_DMA_STRIDE, stride);
    Xil_Out32(GPU_DMA_CMD, 0x01);
    while (Xil_In32(GPU_DMA_STATUS) & 1);
}

static void vdma_to_ddr(uint32_t entry, uint32_t ddr, uint32_t x_size, uint32_t y_size, uint32_t stride)
{
    Xil_Out32(GPU_DMA_SRC, entry);
    Xil_Out32(GPU_DMA_DST, ddr);
    Xil_Out32(GPU_DMA_X_SIZE, x_size);
    Xil_Out32(GPU_DMA_Y_SIZE, y_size);
    Xil_Out32(GPU_DMA_STRIDE, stride);
    Xil_Out32(GPU_DMA_CMD, 0x03);
    while (Xil_In32(GPU_DMA_STATUS) & 1);
}

__attribute__((unused)) static void acc_clear(void)
{
    Xil_Out32(GPU_ACC_CLR, 1);
    for (volatile int i = 0; i < 100; i++);
}

static void dma_acc_flush(uint32_t ddr, uint32_t n)
{
    Xil_Out32(GPU_DMA_SRC, 0);  
    Xil_Out32(GPU_DMA_DST, ddr);
    Xil_Out32(GPU_DMA_X_SIZE, n);
    Xil_Out32(GPU_DMA_Y_SIZE, 1);
    Xil_Out32(GPU_DMA_STRIDE, n * 32);
    Xil_Out32(GPU_DMA_CMD, 0x07);  
    while (Xil_In32(GPU_DMA_STATUS) & 1);
}

static void vdma_acc_flush(uint32_t ddr, uint32_t x_size, uint32_t y_size, uint32_t stride)
{
    Xil_Out32(GPU_DMA_SRC, 0);
    Xil_Out32(GPU_DMA_DST, ddr);
    Xil_Out32(GPU_DMA_X_SIZE, x_size);
    Xil_Out32(GPU_DMA_Y_SIZE, y_size);
    Xil_Out32(GPU_DMA_STRIDE, stride);
    Xil_Out32(GPU_DMA_CMD, 0x07);
    while (Xil_In32(GPU_DMA_STATUS) & 1);
}

static void gpu_upload(const uint32_t *prog, int len)
{
    for (int i = 0; i < len; i++)
        Xil_Out32(GPU_IMEM_BASE + i * 4, prog[i]);
}

static inline void gpu_patch_imem_word(uint32_t pc, uint32_t instr)
{
    Xil_Out32(GPU_IMEM_BASE + pc * 4, instr);
}

static uint64_t gpu_run(void)
{
    uint64_t t0 = timer_now();
    Xil_Out32(GPU_CTRL, 1);
    while (!(Xil_In32(GPU_STATUS) & 1));
    uint64_t t1 = timer_now();
    Xil_Out32(GPU_CTRL, 0);
    return t1 - t0;
}

static void bench_dma(void)
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

static void bench_vecadd_run(void)
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

    
    volatile uint32_t *a = (volatile uint32_t *)DDR_BUF_A;
    volatile uint32_t *b = (volatile uint32_t *)(DDR_BUF_A + 256 * 32);
    volatile uint32_t *c = (volatile uint32_t *)DDR_BUF_B;
    uint64_t t0a = timer_now();
    for (uint32_t i = 0; i < 256; i++) {
        for (int l = 0; l < NUM_LANES; l++)
            c[i * 8 + l] = a[i * 8 + l] + b[i * 8 + l];
    }
    uint64_t t1a = timer_now();
    uint32_t us_arm = ticks_to_us(t1a - t0a);
    xil_printf("  ARM equivalent: %d us (pure compute, data in DDR)\n\r", us_arm);
}

static void bench_conv2d(void)
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

    
    volatile uint32_t *inp = (volatile uint32_t *)DDR_BUF_A;
    volatile uint32_t *out = (volatile uint32_t *)DDR_BUF_B;
    int w[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    uint64_t t0 = timer_now();
    for (int oy = 0; oy < 3; oy++) {
        for (int ox = 0; ox < 3; ox++) {
            uint32_t acc = 0;
            for (int ky = 0; ky < 3; ky++)
                for (int kx = 0; kx < 3; kx++)
                    acc += inp[(oy + ky) * 5 * 8 + (ox + kx) * 8] * w[ky * 3 + kx];
            out[(oy * 3 + ox) * 8] = acc;
        }
    }
    uint64_t t1 = timer_now();
    xil_printf("  ARM equivalent (1 lane): %d us\n\r", ticks_to_us(t1 - t0));
}

static void bench_matmul_run(void)
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

    
    volatile uint32_t *arm_a = (volatile uint32_t *)DDR_BUF_A;
    volatile uint32_t *arm_b = (volatile uint32_t *)(DDR_BUF_A + 64 * 32);
    volatile uint32_t *arm_c = (volatile uint32_t *)DDR_BUF_B;
    uint64_t t0 = timer_now();
    for (int i = 0; i < 8; i++) {
        for (int j = 0; j < 8; j++) {
            uint32_t acc = 0;
            for (int k = 0; k < 8; k++)
                acc += arm_a[i * 8 * 8 + k * 8] * arm_b[k * 8 * 8 + j * 8];
            arm_c[(i * 8 + j) * 8] = acc;
        }
    }
    uint64_t t1 = timer_now();
    xil_printf("  ARM equivalent (1 lane): %d us\n\r", ticks_to_us(t1 - t0));
}

static void bench_matmul_tiled_run(void)
{
    xil_printf("\n\r========================================\n\r");
    xil_printf("  Benchmark 5: Tiled MatMul 16x16 (HW Accumulator)\n\r");
    xil_printf("========================================\n\r");

    
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
            Xil_Out32(GPU_CTRL, 1);

            {
                uint32_t src_A_next = DDR_BUF_A + (ti * 8 * 16 + 1 * 8) * 32;
                uint32_t src_B_next = DDR_BUF_B + (1 * 8 * 16 + tj * 8) * 32;
                vdma_to_dmem(src_A_next, base_by_tk[1],      8, 8, stride);
                vdma_to_dmem(src_B_next, base_by_tk[1] + 64, 8, 8, stride);
            }

            while (!(Xil_In32(GPU_STATUS) & 1));
            uint64_t t1 = timer_now();
            Xil_Out32(GPU_CTRL, 0);
            total_gpu_ticks += (t1 - t0);

            for (int l = 0; l < NUM_LANES; l++)
                DMEM_WR(PARAM_ENTRY, l, base_by_tk[1]);
            gpu_patch_imem_word(patch_pc_mul0, instr_mac_acc);

            t0 = timer_now();
            Xil_Out32(GPU_CTRL, 1);
            while (!(Xil_In32(GPU_STATUS) & 1));
            t1 = timer_now();
            Xil_Out32(GPU_CTRL, 0);
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
    
        
    uint64_t t0a = timer_now();
    volatile uint32_t *arm_c = (volatile uint32_t *)DDR_BUF_B;
    for (int i = 0; i < 16; i++) {
        for (int j = 0; j < 16; j++) {
            uint32_t acc = 0;
            for (int k = 0; k < 16; k++)
                acc += (uint32_t)(i * 16 + k + 1) * (uint32_t)(k * 16 + j + 1);
            arm_c[(i * 16 + j) * 8] = acc;
        }
    }
    uint64_t t1a = timer_now();
    xil_printf("  ARM equivalent (1 lane): %d us\n\r", ticks_to_us(t1a - t0a));
}

static void bench_vdma_2d(void)
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

static void bench_mnist_fc(void)
{
    xil_printf("\n\r========================================\n\r");
    xil_printf("  Benchmark 6: MNIST FC (ARM blocked vs fast)\n\r");
    xil_printf("========================================\n\r");
    xil_printf("  Note: DCache enabled for this benchmark\n\r");

    // Keep DMA-related benchmarks behavior unchanged; only enable cache here.
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

    // --- Blocked reference path (current layout, slower) ---
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

    // --- Fast path (contiguous weight layout) ---
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

static void print_summary(void)
{
    xil_printf("\n\r========================================\n\r");
    xil_printf("  System Specification\n\r");
    xil_printf("========================================\n\r");
    xil_printf("  SoC:        Zynq-7020 (xc7z020clg400-2)\n\r");
    xil_printf("  ARM:        Cortex-A9 @ 667 MHz\n\r");
    xil_printf("  GPU Clock:  50 MHz (FCLK_CLK0)\n\r");
    xil_printf("  SIMD Lanes: 8 x 32-bit integer\n\r");
    xil_printf("  Pipeline:   Fetch(comb) -> Decode(comb) -> Execute(1-clk) + Forwarding\n\r");
    xil_printf("  IMEM:       1024 x 32-bit instructions\n\r");
    xil_printf("  DMEM:       4096 entries x 8 lanes x 32-bit = 128 KB\n\r");
    xil_printf("  ISA:        18 instructions (ALU/MAC/LDR/STR/Branch/Jump + MUL_OVR + MAC_ACC_NXT)\n\r");
    xil_printf("  DMA:        AXI4 Master via S_AXI_HP0, burst DDR<->DMEM\n\r");
    xil_printf("  Peak:       50M instr/s x 8 lanes = 400 MOPS (theoretical)\n\r");
    xil_printf("  Peak MAC:   50M MAC/s x 8 lanes = 400 MMAC/s (theoretical)\n\r");
}

int main()
{
    Xil_DCacheDisable();
    timer_init();
    Xil_Out32(GPU_CTRL, 0);

    xil_printf("\n\r");
    xil_printf("########################################\n\r");
    xil_printf("#  Zynq GPGPU Performance Benchmark    #\n\r");
    xil_printf("########################################\n\r");

    print_summary();
    bench_dma();
    bench_vdma_2d();
    bench_vecadd_run();
    bench_conv2d();
    bench_matmul_run();
    bench_matmul_tiled_run();
    bench_mnist_fc();

    xil_printf("\n\r########################################\n\r");
    xil_printf("#  Benchmark Complete                  #\n\r");
    xil_printf("########################################\n\r");

    return 0;
}
