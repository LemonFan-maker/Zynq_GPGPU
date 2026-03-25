#pragma once

#include <stdint.h>

#include "xil_io.h"
#include "xil_printf.h"

#define GT_COUNTER_LO  0xF8F00200
#define GT_COUNTER_HI  0xF8F00204
#define GT_CONTROL     0xF8F00208
#define TIMER_FREQ_HZ  333333333ULL

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
#define GPU_DBG_STARTS (GPU_BASE + 0x028)
#define GPU_DBG_DONES  (GPU_BASE + 0x02C)
#define GPU_DBG_LASTPC (GPU_BASE + 0x030)
#define GPU_DBG_FLAGS  (GPU_BASE + 0x034)
#define GPU_BUILD_ID   (GPU_BASE + 0x0FC)
#define GPU_EXPECT_BUILD_ID 0x26032501U
#define GPU_IMEM_BASE  (GPU_BASE + 0x100)
#define GPU_DMEM_BASE  (GPU_BASE + 0x2000)

#define NUM_LANES 16
#define BYTES_PER_ENTRY (NUM_LANES * 4U)
#define DMEM_WR(e,l,v) Xil_Out32(GPU_DMEM_BASE + (e) * BYTES_PER_ENTRY + (l) * 4U, (v))
#define DMEM_RD(e,l)   Xil_In32(GPU_DMEM_BASE + (e) * BYTES_PER_ENTRY + (l) * 4U)

#define DDR_BUF_A  0x10000000
#define DDR_BUF_B  0x10100000
#define DDR_STAGE  0x10200000
#define DDR_C_OUT  0x10300000

#define PARAM_ENTRY 250
#define PING_BASE   0
#define PONG_BASE   512
#define GPU_RUN_TIMEOUT_US 200000U

void timer_init(void);
uint64_t timer_now(void);
uint32_t ticks_to_us(uint64_t dt);

void dma_to_dmem(uint32_t ddr, uint32_t entry, uint32_t n);
void dma_to_ddr(uint32_t entry, uint32_t ddr, uint32_t n);
void vdma_to_dmem(uint32_t ddr, uint32_t entry, uint32_t x_size, uint32_t y_size, uint32_t stride);
void vdma_to_ddr(uint32_t entry, uint32_t ddr, uint32_t x_size, uint32_t y_size, uint32_t stride);

void dma_acc_flush(uint32_t ddr, uint32_t n);
void vdma_acc_flush(uint32_t ddr, uint32_t x_size, uint32_t y_size, uint32_t stride);

void gpu_upload(const uint32_t *prog, int len);
void gpu_patch_imem_word(uint32_t pc, uint32_t instr);
void gpu_start(void);
void gpu_stop(void);
int gpu_wait_done_edge(uint32_t timeout_us);
int gpu_wait_done_edge_from(uint32_t timeout_us, uint32_t done_cnt_before);
uint64_t gpu_run(void);
void gpu_debug_dump_state(const char *tag);
void gpu_debug_dump_imem(int words);

void print_summary(void);
