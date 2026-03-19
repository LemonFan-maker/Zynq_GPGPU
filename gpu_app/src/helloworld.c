#include "xil_printf.h"
#include "xil_io.h"
#include "xil_cache.h"
#include "conv2d_3x3.h"

#define GPU_BASE_ADDR  0x40000000
#define GPU_CTRL_REG   (GPU_BASE_ADDR + 0x000)
#define GPU_STATUS_REG (GPU_BASE_ADDR + 0x004)
#define GPU_IMEM_BASE  (GPU_BASE_ADDR + 0x100)
#define GPU_DMEM_BASE  (GPU_BASE_ADDR + 0x2000)

#define GPU_DMA_SRC    (GPU_BASE_ADDR + 0x008)
#define GPU_DMA_DST    (GPU_BASE_ADDR + 0x00C)
#define GPU_DMA_LEN    (GPU_BASE_ADDR + 0x010)
#define GPU_DMA_CMD    (GPU_BASE_ADDR + 0x014)
#define GPU_DMA_STATUS (GPU_BASE_ADDR + 0x018)

#define DMEM_WRITE(entry, lane, val) \
    Xil_Out32(GPU_DMEM_BASE + (entry)*16 + (lane)*4, (val))
#define DMEM_READ(entry, lane) \
    Xil_In32(GPU_DMEM_BASE + (entry)*16 + (lane)*4)

#define DDR_INPUT_BUF   0x10000000
#define DDR_OUTPUT_BUF  0x10100000

/* DMA helpers */
static void dma_ddr_to_dmem(uint32_t ddr_addr, uint32_t dmem_entry, uint32_t num_entries)
{
    Xil_Out32(GPU_DMA_SRC, ddr_addr);
    Xil_Out32(GPU_DMA_DST, dmem_entry);
    Xil_Out32(GPU_DMA_LEN, num_entries);
    Xil_Out32(GPU_DMA_CMD, 0x01);          /* dir=0, start=1 */
    while (Xil_In32(GPU_DMA_STATUS) & 1);  /* wait busy clear */
}

static void dma_dmem_to_ddr(uint32_t dmem_entry, uint32_t ddr_addr, uint32_t num_entries)
{
    Xil_Out32(GPU_DMA_SRC, dmem_entry);
    Xil_Out32(GPU_DMA_DST, ddr_addr);
    Xil_Out32(GPU_DMA_LEN, num_entries);
    Xil_Out32(GPU_DMA_CMD, 0x03);          /* dir=1, start=1 */
    while (Xil_In32(GPU_DMA_STATUS) & 1);
}

static const uint32_t expected[9] = {
    411, 456, 501,
    636, 681, 726,
    861, 906, 951
};

int main()
{
    Xil_DCacheDisable();
    Xil_Out32(GPU_CTRL_REG, 0);

    xil_printf("=== DMA Round Trip Test ===\n\r");

    for (int i = 0; i < 25; i++)
        for (int l = 0; l < 4; l++)
            Xil_Out32(DDR_INPUT_BUF + i * 16 + l * 4, i + 1);

    dma_ddr_to_dmem(DDR_INPUT_BUF, 0, 25);

    int dma_pass = 0, dma_fail = 0;
    for (int i = 0; i < 25; i++) {
        for (int l = 0; l < 4; l++) {
            uint32_t got = DMEM_READ(i, l);
            if (got == (uint32_t)(i + 1))
                dma_pass++;
            else if (++dma_fail <= 5)
                xil_printf("  FAIL: DMEM[%d][%d] = %d, expected %d\n\r",
                           i, l, got, i + 1);
        }
    }
    xil_printf("DMA load verify: %d/100 PASS", dma_pass);
    if (dma_fail == 0) xil_printf(" --- ALL PASS ---");
    xil_printf("\n\r");

    xil_printf("\n\r=== Conv2D 3x3 Test (DMA Method) ===\n\r");

    for (int i = 50; i < 59; i++)
        for (int l = 0; l < 4; l++)
            DMEM_WRITE(i, l, 0);

    for (int i = 0; i < conv2d_3x3_LEN; i++)
        Xil_Out32(GPU_IMEM_BASE + i * 4, conv2d_3x3[i]);

    Xil_Out32(GPU_CTRL_REG, 1);
    while (!(Xil_In32(GPU_STATUS_REG) & 1));
    Xil_Out32(GPU_CTRL_REG, 0);

    dma_dmem_to_ddr(50, DDR_OUTPUT_BUF, 9);

    int pass = 0, fail = 0;
    for (int oy = 0; oy < 3; oy++) {
        for (int ox = 0; ox < 3; ox++) {
            int idx = oy * 3 + ox;
            uint32_t got = Xil_In32(DDR_OUTPUT_BUF + idx * 16);
            uint32_t exp = expected[idx];
            if (got == exp) {
                xil_printf("  out[%d][%d] = %d  PASS\n\r", oy, ox, got);
                pass++;
            } else {
                xil_printf("  out[%d][%d] = %d  FAIL (exp %d)\n\r", oy, ox, got, exp);
                fail++;
            }
        }
    }
    xil_printf("--- Score: %d/9 PASS", pass);
    if (fail == 0) xil_printf(" --- ALL PASS ---");
    xil_printf("\n\r");

    int lane_mismatch = 0;
    for (int idx = 0; idx < 9; idx++) {
        uint32_t v0 = Xil_In32(DDR_OUTPUT_BUF + idx * 16);
        for (int l = 1; l < 4; l++)
            if (Xil_In32(DDR_OUTPUT_BUF + idx * 16 + l * 4) != v0)
                lane_mismatch++;
    }
    xil_printf("Lane check: %s\n\r",
               lane_mismatch == 0 ? "all 4 lanes consistent" : "MISMATCH!");

    return 0;
}
