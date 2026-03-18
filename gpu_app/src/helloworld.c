#include <stdio.h>
#include "xil_printf.h"
#include "xil_io.h"
#include "xil_cache.h"
#include "mac_dot.h"

#define GPU_BASE_ADDR  0x40000000
#define GPU_CTRL_REG   (GPU_BASE_ADDR + 0x000)
#define GPU_IMEM_BASE  (GPU_BASE_ADDR + 0x100)
#define GPU_DMEM_BASE  (GPU_BASE_ADDR + 0x1000)

#define DMEM_WRITE(entry, lane, val) \
    Xil_Out32(GPU_DMEM_BASE + (entry)*16 + (lane)*4, (val))
#define DMEM_READ(entry, lane) \
    Xil_In32(GPU_DMEM_BASE + (entry)*16 + (lane)*4)

int main()
{
    Xil_DCacheDisable();

    // 停止GPU
    Xil_Out32(GPU_CTRL_REG, 0);

    // 上传程序
    xil_printf("Uploading program (%d instructions)...\n\r", mac_dot_LEN);
    for (int i = 0; i < mac_dot_LEN; i++)
        Xil_Out32(GPU_IMEM_BASE + i*4, mac_dot[i]);

    // 初始化DMEM数据
    // A = {1,2,3,4} @ DMEM[100..103]
    // B = {5,6,7,8} @ DMEM[104..107]
    for (int l = 0; l < 4; l++) {
        DMEM_WRITE(100, l, 1);
        DMEM_WRITE(101, l, 2);
        DMEM_WRITE(102, l, 3);
        DMEM_WRITE(103, l, 4);
        DMEM_WRITE(104, l, 5);
        DMEM_WRITE(105, l, 6);
        DMEM_WRITE(106, l, 7);
        DMEM_WRITE(107, l, 8);
        DMEM_WRITE(93, l, 0);
    }

    // 启动GPU
    xil_printf("Running GPU...\n\r");
    Xil_Out32(GPU_CTRL_REG, 1);

    // 等待执行完毕
    for (volatile int i = 0; i < 30000; i++);

    // 停止 GPU
    Xil_Out32(GPU_CTRL_REG, 0);

    // 读回结果
    uint32_t result = DMEM_READ(93, 0);
    xil_printf("Result: %d\n\r", result);

    return 0;
}
