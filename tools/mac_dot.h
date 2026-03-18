// 由 gpuasm 从 mac_dot.asm 之中生成
#include <stdint.h>

uint32_t mac_dot[] = {
    0xC0800000,  // [0] ADDI r1, r0, 0          ; sum = 0
    0xC1000000,  // [1] ADDI r2, r0, 0          ; i = 0
    0xC1800004,  // [2] ADDI r3, r0, 4          ; loop count
    0xC2000064,  // [3] ADDI r4, r0, 100        ; A base
    0xC2800068,  // [4] ADDI r5, r0, 104        ; B base
    0x83000400,  // [5] LDR  r6, [r4 + 0]       ; A[i]
    0x83000400,  // [6] LDR  r6, [r4 + 0]
    0x83800500,  // [7] LDR  r7, [r5 + 0]       ; B[i]
    0x83800500,  // [8] LDR  r7, [r5 + 0]
    0x00000000,  // [9] NOP
    0x2080E601,  // [10] MAC  r1, r6, r7         ; sum += A[i]*B[i]
    0xC2000401,  // [11] ADDI r4, r4, 1
    0xC2800501,  // [12] ADDI r5, r5, 1
    0xC1000201,  // [13] ADDI r2, r2, 1
    0x00000000,  // [14] NOP
    0xE183E2F6,  // [15] BNE  r2, r3, loop
    0x00000000,  // [16] NOP
    0x9000205D,  // [17] STR  r1, [r0 + 93]
};
#define mac_dot_LEN 18
