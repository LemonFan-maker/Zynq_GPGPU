#include <stdio.h>
#include "xil_printf.h"
#include "xil_io.h"
#include "xil_cache.h"
#include "conv2d_3x3.h"

#define GPU_BASE_ADDR  0x40000000
#define GPU_CTRL_REG   (GPU_BASE_ADDR + 0x000)
#define GPU_STATUS_REG (GPU_BASE_ADDR + 0x004)
#define GPU_IMEM_BASE  (GPU_BASE_ADDR + 0x100)
#define GPU_DMEM_BASE  (GPU_BASE_ADDR + 0x1000)

#define DMEM_WRITE(entry, lane, val) \
    Xil_Out32(GPU_DMEM_BASE + (entry)*16 + (lane)*4, (val))
#define DMEM_READ(entry, lane) \
    Xil_In32(GPU_DMEM_BASE + (entry)*16 + (lane)*4)

/* Expected conv2d output (hand-verified) */
static const uint32_t expected[9] = {
    411, 456, 501,
    636, 681, 726,
    861, 906, 951
};

int main()
{
    Xil_DCacheDisable();

    Xil_Out32(GPU_CTRL_REG, 0);

    xil_printf("=== Conv2D 3x3 Test ===\n\r");
    xil_printf("Uploading program (%d instructions)...\n\r", conv2d_3x3_LEN);
    for (int i = 0; i < conv2d_3x3_LEN; i++)
        Xil_Out32(GPU_IMEM_BASE + i * 4, conv2d_3x3[i]);

    xil_printf("Initializing 5x5 input (1-25)...\n\r");
    for (int i = 0; i < 25; i++)
        for (int l = 0; l < 4; l++)
            DMEM_WRITE(i, l, i + 1);

    for (int i = 50; i < 59; i++)
        for (int l = 0; l < 4; l++)
            DMEM_WRITE(i, l, 0);

    xil_printf("Running GPU...\n\r");
    Xil_Out32(GPU_CTRL_REG, 1);

    while (!(Xil_In32(GPU_STATUS_REG) & 1));

    Xil_Out32(GPU_CTRL_REG, 0);

    int pass = 0;
    int fail = 0;

    xil_printf("--- Results ---\n\r");
    for (int oy = 0; oy < 3; oy++) {
        for (int ox = 0; ox < 3; ox++) {
            int idx = oy * 3 + ox;
            uint32_t got = DMEM_READ(50 + idx, 0);
            uint32_t exp = expected[idx];
            if (got == exp) {
                xil_printf("  out[%d][%d] = %d  PASS\n\r", oy, ox, got);
                pass++;
            } else {
                xil_printf("  out[%d][%d] = %d FAIL\n\r",
                           oy, ox, got, exp);
                fail++;
            }
        }
    }

    xil_printf("--- Score: %d/9 PASS", pass);
    if (fail == 0)
        xil_printf("\n --- ALL PASS ---");
    xil_printf("\n\r");

    int lane_mismatch = 0;
    for (int idx = 0; idx < 9; idx++) {
        uint32_t v0 = DMEM_READ(50 + idx, 0);
        for (int l = 1; l < 4; l++) {
            if (DMEM_READ(50 + idx, l) != v0)
                lane_mismatch++;
        }
    }
    if (lane_mismatch == 0)
        xil_printf("Lane check: all 4 lanes consistent\n\r");
    else
        xil_printf("Lane check: %d mismatches!\n\r", lane_mismatch);

    return 0;
}
