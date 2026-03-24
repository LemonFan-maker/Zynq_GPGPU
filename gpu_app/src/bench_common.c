#include "bench_common.h"

#ifndef GPU_UPLOAD_VERIFY
#define GPU_UPLOAD_VERIFY 1
#endif

void timer_init(void)
{
    Xil_Out32(GT_CONTROL, Xil_In32(GT_CONTROL) | 1);
}

uint64_t timer_now(void)
{
    uint32_t hi, lo, hi2;
    do {
        hi  = Xil_In32(GT_COUNTER_HI);
        lo  = Xil_In32(GT_COUNTER_LO);
        hi2 = Xil_In32(GT_COUNTER_HI);
    } while (hi != hi2);
    return ((uint64_t)hi << 32) | lo;
}

uint32_t ticks_to_us(uint64_t dt)
{
    return (uint32_t)(dt / (TIMER_FREQ_HZ / 1000000));
}

void dma_to_dmem(uint32_t ddr, uint32_t entry, uint32_t n)
{
    Xil_Out32(GPU_DMA_SRC, ddr);
    Xil_Out32(GPU_DMA_DST, entry);
    Xil_Out32(GPU_DMA_X_SIZE, n);
    Xil_Out32(GPU_DMA_Y_SIZE, 1);
    Xil_Out32(GPU_DMA_STRIDE, n * 32);
    Xil_Out32(GPU_DMA_CMD, 0x01);
    while (Xil_In32(GPU_DMA_STATUS) & 1);
}

void dma_to_ddr(uint32_t entry, uint32_t ddr, uint32_t n)
{
    Xil_Out32(GPU_DMA_SRC, entry);
    Xil_Out32(GPU_DMA_DST, ddr);
    Xil_Out32(GPU_DMA_X_SIZE, n);
    Xil_Out32(GPU_DMA_Y_SIZE, 1);
    Xil_Out32(GPU_DMA_STRIDE, n * 32);
    Xil_Out32(GPU_DMA_CMD, 0x03);
    while (Xil_In32(GPU_DMA_STATUS) & 1);
}

void vdma_to_dmem(uint32_t ddr, uint32_t entry, uint32_t x_size, uint32_t y_size, uint32_t stride)
{
    Xil_Out32(GPU_DMA_SRC, ddr);
    Xil_Out32(GPU_DMA_DST, entry);
    Xil_Out32(GPU_DMA_X_SIZE, x_size);
    Xil_Out32(GPU_DMA_Y_SIZE, y_size);
    Xil_Out32(GPU_DMA_STRIDE, stride);
    Xil_Out32(GPU_DMA_CMD, 0x01);
    while (Xil_In32(GPU_DMA_STATUS) & 1);
}

void vdma_to_ddr(uint32_t entry, uint32_t ddr, uint32_t x_size, uint32_t y_size, uint32_t stride)
{
    Xil_Out32(GPU_DMA_SRC, entry);
    Xil_Out32(GPU_DMA_DST, ddr);
    Xil_Out32(GPU_DMA_X_SIZE, x_size);
    Xil_Out32(GPU_DMA_Y_SIZE, y_size);
    Xil_Out32(GPU_DMA_STRIDE, stride);
    Xil_Out32(GPU_DMA_CMD, 0x03);
    while (Xil_In32(GPU_DMA_STATUS) & 1);
}

void dma_acc_flush(uint32_t ddr, uint32_t n)
{
    Xil_Out32(GPU_DMA_SRC, 0);
    Xil_Out32(GPU_DMA_DST, ddr);
    Xil_Out32(GPU_DMA_X_SIZE, n);
    Xil_Out32(GPU_DMA_Y_SIZE, 1);
    Xil_Out32(GPU_DMA_STRIDE, n * 32);
    Xil_Out32(GPU_DMA_CMD, 0x07);
    while (Xil_In32(GPU_DMA_STATUS) & 1);
}

void vdma_acc_flush(uint32_t ddr, uint32_t x_size, uint32_t y_size, uint32_t stride)
{
    Xil_Out32(GPU_DMA_SRC, 0);
    Xil_Out32(GPU_DMA_DST, ddr);
    Xil_Out32(GPU_DMA_X_SIZE, x_size);
    Xil_Out32(GPU_DMA_Y_SIZE, y_size);
    Xil_Out32(GPU_DMA_STRIDE, stride);
    Xil_Out32(GPU_DMA_CMD, 0x07);
    while (Xil_In32(GPU_DMA_STATUS) & 1);
}

void gpu_debug_dump_state(const char *tag)
{
    xil_printf("  [GPU-DBG] %s\n\r", tag);
    xil_printf("    BUILD_ID=0x%08x CTRL=0x%08x STATUS=0x%08x\n\r",
               Xil_In32(GPU_BUILD_ID), Xil_In32(GPU_CTRL), Xil_In32(GPU_STATUS));
    xil_printf("    DMA: SRC=0x%08x DST=0x%08x X=%u Y=%u STRIDE=%u CMD=0x%08x BUSY=0x%08x\n\r",
               Xil_In32(GPU_DMA_SRC), Xil_In32(GPU_DMA_DST),
               Xil_In32(GPU_DMA_X_SIZE), Xil_In32(GPU_DMA_Y_SIZE),
               Xil_In32(GPU_DMA_STRIDE), Xil_In32(GPU_DMA_CMD), Xil_In32(GPU_DMA_STATUS));
    xil_printf("    CORE: START_CNT=%u DONE_CNT=%u LAST_PC=%u FLAGS=0x%08x\n\r",
               Xil_In32(GPU_DBG_STARTS), Xil_In32(GPU_DBG_DONES),
               Xil_In32(GPU_DBG_LASTPC), Xil_In32(GPU_DBG_FLAGS));
}

void gpu_debug_dump_imem(int words)
{
    if (words <= 0)
        return;
    if (words > 16)
        words = 16;

    xil_printf("  [GPU-DBG] IMEM[0..%d):\n\r", words);
    for (int i = 0; i < words; i++) {
        xil_printf("    IMEM[%d] = 0x%08x\n\r", i, Xil_In32(GPU_IMEM_BASE + (uint32_t)i * 4U));
    }
}

void gpu_upload(const uint32_t *prog, int len)
{
    for (int i = 0; i < len; i++)
        Xil_Out32(GPU_IMEM_BASE + i * 4, prog[i]);

#if GPU_UPLOAD_VERIFY
    int mismatches = 0;
    for (int i = 0; i < len; i++) {
        uint32_t got = Xil_In32(GPU_IMEM_BASE + i * 4);
        if (got != prog[i]) {
            if (mismatches < 4) {
                xil_printf("  [GPU-DBG] IMEM mismatch @%d: got 0x%08x exp 0x%08x\n\r", i, got, prog[i]);
            }
            mismatches++;
        }
    }
    if (mismatches) {
        xil_printf("  [GPU-DBG] IMEM verify failed: %d/%d mismatches\n\r", mismatches, len);
        gpu_debug_dump_state("after gpu_upload");
    }
#endif
}

void gpu_patch_imem_word(uint32_t pc, uint32_t instr)
{
    Xil_Out32(GPU_IMEM_BASE + pc * 4, instr);
}

static inline void gpu_ctrl_write(uint32_t v)
{
    Xil_Out32(GPU_CTRL, v);
    (void)Xil_In32(GPU_CTRL);
}

void gpu_start(void)
{
    gpu_ctrl_write(1);
}

void gpu_stop(void)
{
    gpu_ctrl_write(0);
}

int gpu_wait_done_edge_from(uint32_t timeout_us, uint32_t done_cnt_before)
{
    const uint64_t timeout_ticks = ((uint64_t)timeout_us * TIMER_FREQ_HZ) / 1000000ULL;
    uint64_t t0 = timer_now();
    int saw_done_clear = ((Xil_In32(GPU_STATUS) & 1U) == 0U) ? 1 : 0;

    while ((Xil_In32(GPU_DBG_DONES) - done_cnt_before) == 0U) {
        if ((Xil_In32(GPU_STATUS) & 1U) == 0U) {
            saw_done_clear = 1;
        }
        if ((timer_now() - t0) > timeout_ticks) {
            return saw_done_clear ? -2 : -1;
        }
    }

    return 0;
}

int gpu_wait_done_edge(uint32_t timeout_us)
{
    return gpu_wait_done_edge_from(timeout_us, Xil_In32(GPU_DBG_DONES));
}

uint64_t gpu_run(void)
{
    gpu_stop();
    uint32_t done_before = Xil_In32(GPU_DBG_DONES);
    uint64_t t0 = timer_now();
    gpu_start();
    int rc = gpu_wait_done_edge_from(GPU_RUN_TIMEOUT_US, done_before);
    uint64_t t1 = timer_now();

    if (rc == -1) {
        xil_printf("  [GPU] timeout waiting DONE clear (stale done)\n\r");
        xil_printf("  [GPU-DBG] DONE_CNT before=%u after=%u\n\r", done_before, Xil_In32(GPU_DBG_DONES));
        gpu_debug_dump_state("timeout: stale done");
        gpu_debug_dump_imem(8);
    } else if (rc == -2) {
        xil_printf("  [GPU] timeout waiting DONE assert (kernel hang)\n\r");
        xil_printf("  [GPU-DBG] DONE_CNT before=%u after=%u\n\r", done_before, Xil_In32(GPU_DBG_DONES));
        gpu_debug_dump_state("timeout: kernel hang");
        gpu_debug_dump_imem(8);
    }

    gpu_stop();
    return t1 - t0;
}

void print_summary(void)
{
    uint32_t build_id = Xil_In32(GPU_BUILD_ID);

    xil_printf("\n\r========================================\n\r");
    xil_printf("  System Specification\n\r");
    xil_printf("========================================\n\r");
    xil_printf("  SoC:        Zynq-7020 (xc7z020clg400-2)\n\r");
    xil_printf("  ARM:        Cortex-A9 @ 667 MHz\n\r");
    xil_printf("  GPU Clock:  75 MHz (FCLK_CLK0)\n\r");
    xil_printf("  SIMD Lanes: 8 x 32-bit integer\n\r");
    xil_printf("  Pipeline:   Fetch(comb) -> Decode(comb) -> Execute(1-clk) + Forwarding\n\r");
    xil_printf("  IMEM:       1024 x 32-bit instructions\n\r");
    xil_printf("  DMEM:       4096 entries x 8 lanes x 32-bit = 128 KB\n\r");
    xil_printf("  ISA:        +DP4A extension enabled\n\r");
    xil_printf("  DMA:        AXI4 Master via S_AXI_HP0, burst DDR<->DMEM\n\r");
    xil_printf("  GPU Build:  0x%08x\n\r", build_id);
    if (build_id != GPU_EXPECT_BUILD_ID) {
        xil_printf("  [WARN] Build ID mismatch (exp 0x%08x). Bitstream/app may be out of sync.\n\r",
                   GPU_EXPECT_BUILD_ID);
    }
    xil_printf("  Peak:       75M instr/s x 8 lanes = 600 MOPS (theoretical)\n\r");
    xil_printf("  Peak MAC:   75M MAC/s x 8 lanes = 600 MMAC/s (theoretical)\n\r");
    xil_printf("  Peak INT8:  75M DP4A/s x 8 lanes x 4 = 2400 IMAC/s (theoretical)\n\r");
}
