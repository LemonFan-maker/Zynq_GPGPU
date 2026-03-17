#include <stdio.h>
#include "xil_printf.h"
#include "xil_io.h"
#include "xil_cache.h"

#define GPU_BASE_ADDR  0x40000000  
#define GPU_CTRL_REG   (GPU_BASE_ADDR + 0x00000)
#define GPU_IMEM_BASE  (GPU_BASE_ADDR + 0x10000)
#define GPU_DMEM_BASE  (GPU_BASE_ADDR + 0x20000)

#define TRASH_NOP 0x02000000 

int main()
{
    Xil_DCacheDisable(); 

    xil_printf("ZYNQ GPGPU  SIMT Execution \n\r");

    Xil_Out32(GPU_CTRL_REG, 0x00000000); 
    
    xil_printf("[ARM] Injecting Data to Expanded VRAM...\n\r");
    
    // 初始化DMEM[10]
    Xil_Out32(GPU_DMEM_BASE + 10*16 + 0*4, 10);  
    Xil_Out32(GPU_DMEM_BASE + 10*16 + 1*4, 100); 
    Xil_Out32(GPU_DMEM_BASE + 10*16 + 2*4, 30);  
    Xil_Out32(GPU_DMEM_BASE + 10*16 + 3*4, 100); 

    // 初始化DMEM[14]
    Xil_Out32(GPU_DMEM_BASE + 14*16 + 0*4, 100);
    Xil_Out32(GPU_DMEM_BASE + 14*16 + 1*4, 100);
    Xil_Out32(GPU_DMEM_BASE + 14*16 + 2*4, 100);
    Xil_Out32(GPU_DMEM_BASE + 14*16 + 3*4, 100);

    // 清空结果区域DMEM[20]，
    Xil_Out32(GPU_DMEM_BASE + 20*16 + 0*4, 0);
    Xil_Out32(GPU_DMEM_BASE + 20*16 + 1*4, 0);
    Xil_Out32(GPU_DMEM_BASE + 20*16 + 2*4, 0);
    Xil_Out32(GPU_DMEM_BASE + 20*16 + 3*4, 0);
    
    xil_printf("[ARM] Uploading SIMT Kernel to Expanded IMEM...\n\r");
    
    Xil_Out32(GPU_IMEM_BASE + 0*4, 0x8080000A); // [0] LDR r1, [10]
    Xil_Out32(GPU_IMEM_BASE + 1*4, 0x8080000A); // [1] LDR r1, [10]
    
    Xil_Out32(GPU_IMEM_BASE + 2*4, 0x8100000E); // [2] LDR r2, [14]
    Xil_Out32(GPU_IMEM_BASE + 3*4, 0x8100000E); // [3] LDR r2, [14]
    
    Xil_Out32(GPU_IMEM_BASE + 4*4, TRASH_NOP);  // [4] 等待读取稳定
    
    Xil_Out32(GPU_IMEM_BASE + 5*4, 0xA0004100); // [5] SETM r0, r1, r2
    Xil_Out32(GPU_IMEM_BASE + 6*4, TRASH_NOP);  // [6] 等待掩码生效
    
    Xil_Out32(GPU_IMEM_BASE + 7*4, 0x01802100); // [7] ADD r3, r1, r1 
    Xil_Out32(GPU_IMEM_BASE + 8*4, TRASH_NOP);  // [8] 等待ALU写回
    
    Xil_Out32(GPU_IMEM_BASE + 9*4, 0x90006014); // [9] STR r3, [20]

    for (int i=10; i<20; i++) {
        Xil_Out32(GPU_IMEM_BASE + i*4, TRASH_NOP);
    }
    
    xil_printf("[ARM] Launching SIMT Array!\n\r");
    Xil_Out32(GPU_CTRL_REG, 0x00000001); 

    for(volatile int i=0; i<5000; i++); 

    Xil_Out32(GPU_CTRL_REG, 0x00000000); 

    xil_printf("\n\r[ARM] Reading SIMT Result from DMEM[20]...\n\r");
    uint32_t res_lane0 = Xil_In32(GPU_DMEM_BASE + 20*16 + 0*4);
    uint32_t res_lane1 = Xil_In32(GPU_DMEM_BASE + 20*16 + 1*4);
    uint32_t res_lane2 = Xil_In32(GPU_DMEM_BASE + 20*16 + 2*4);
    uint32_t res_lane3 = Xil_In32(GPU_DMEM_BASE + 20*16 + 3*4);

    xil_printf("------------------------------------------\n\r");
    xil_printf("  [Lane 3] Result : %3d\n\r", res_lane3);
    xil_printf("  [Lane 2] Result : %3d <-Masked\n\r", res_lane2);
    xil_printf("  [Lane 1] Result : %3d\n\r", res_lane1);
    xil_printf("  [Lane 0] Result : %3d <-Masked\n\r", res_lane0);
    xil_printf("------------------------------------------\n\r");

    return 0;
}