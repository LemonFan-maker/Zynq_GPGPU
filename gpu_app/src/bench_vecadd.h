// 由 gpuasm 从 bench_vecadd.asm 之中生成
#include <stdint.h>

uint32_t bench_vecadd[] = {
    0xC2802000,  // [0] ADDI r5, r0, 256     ; loop bound
    0xC3002000,  // [1] ADDI r6, r0, 256     ; B base offset
    0xC2000000,  // [2] ADDI r4, r0, 0       ; i = 0
    0x80800400,  // [3] LDR  r1, [r4 + 0]    ; A[i] = DMEM[i]
    0x0100C400,  // [4] ADD  r2, r4, r6      ; addr_B = i + 256
    0x03800400,  // [5] ADD  r7, r4, r0      ; save i for output addr
    0x81000200,  // [6] LDR  r2, [r2 + 0]    ; B[i] = DMEM[i+256]
    0xC3804700,  // [7] ADDI r7, r7, 512     ; out_addr = saved_i + 512
    0x01804100,  // [8] ADD  r3, r1, r2      ; result = A[i] + B[i]
    0xC2000401,  // [9] ADDI r4, r4, 1       ; i++
    0x90006700,  // [10] STR  r3, [r7 + 0]    ; DMEM[saved_i+512] = result
    0xE283E4F8,  // [11] BNE  r4, r5, loop    ; if i < 256 continue
    0x00000001,  // [12] HALT
};
#define bench_vecadd_LEN 13
