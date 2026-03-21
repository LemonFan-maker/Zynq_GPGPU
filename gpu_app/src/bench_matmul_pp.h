// 由 gpuasm 从 bench_matmul_pp.asm 之中生成
#include <stdint.h>

uint32_t bench_matmul_pp[] = {
    0xC4800008,  // [0] ADDI r9,  r0, 8         ; loop bound = 8
    0xCB8000FA,  // [1] ADDI r23, r0, 250       ; parameter address
    0x8B801700,  // [2] LDR  r23, [r23 + 0]     ; base = DMEM[250]
    0x05001700,  // [3] ADD  r10, r23, r0       ; r10 = base
    0xC5000A40,  // [4] ADDI r10, r10, 64       ; B_base = base + 64
    0xC1000000,  // [5] ADDI r2,  r0, 0         ; i = 0
    0x22012200,  // [6] MUL  r4, r2, r9         ; row offset = i * 8
    0x0202E400,  // [7] ADD  r4, r4, r23        ; A_row_base = base + i*8
    0xC1800000,  // [8] ADDI r3, r0, 0          ; j = 0
    0x02806A00,  // [9] ADD  r5, r10, r3        ; col_base_B = B_base + j
    0x83000400,  // [10] LDR  r6,  [r4 + 0]     ; A[i][0]
    0x83800500,  // [11] LDR  r7,  [r5 + 0]     ; B[0][j]
    0x8A800401,  // [12] LDR  r21, [r4 + 1]     ; A[i][1]
    0x2000E602,  // [13] MAC_ACC r6,  r7         ; acc_buf[ptr] += A[i][0]*B[0][j]
    0x8B000508,  // [14] LDR  r22, [r5 + 8]     ; B[1][j]
    0x83000402,  // [15] LDR  r6,  [r4 + 2]     ; A[i][2]
    0x2002D502,  // [16] MAC_ACC r21, r22        ; acc_buf[ptr] += A[i][1]*B[1][j]
    0x83800510,  // [17] LDR  r7,  [r5 + 16]    ; B[2][j]
    0x8A800403,  // [18] LDR  r21, [r4 + 3]     ; A[i][3]
    0x2000E602,  // [19] MAC_ACC r6,  r7         ; acc_buf[ptr] += A[i][2]*B[2][j]
    0x8B000518,  // [20] LDR  r22, [r5 + 24]    ; B[3][j]
    0x83000404,  // [21] LDR  r6,  [r4 + 4]     ; A[i][4]
    0x2002D502,  // [22] MAC_ACC r21, r22        ; acc_buf[ptr] += A[i][3]*B[3][j]
    0x83800520,  // [23] LDR  r7,  [r5 + 32]    ; B[4][j]
    0x8A800405,  // [24] LDR  r21, [r4 + 5]     ; A[i][5]
    0x2000E602,  // [25] MAC_ACC r6,  r7         ; acc_buf[ptr] += A[i][4]*B[4][j]
    0x8B000528,  // [26] LDR  r22, [r5 + 40]    ; B[5][j]
    0x83000406,  // [27] LDR  r6,  [r4 + 6]     ; A[i][6]
    0x2002D502,  // [28] MAC_ACC r21, r22        ; acc_buf[ptr] += A[i][5]*B[5][j]
    0x83800530,  // [29] LDR  r7,  [r5 + 48]    ; B[6][j]
    0x8A800407,  // [30] LDR  r21, [r4 + 7]     ; A[i][7]
    0x2000E602,  // [31] MAC_ACC r6,  r7         ; acc_buf[ptr] += A[i][6]*B[6][j]
    0x8B000538,  // [32] LDR  r22, [r5 + 56]    ; B[7][j]
    0xC1800301,  // [33] ADDI r3,  r3,  1       ; j++
    0x2002D50A,  // [34] MAC_ACC_NXT r21, r22    ; acc_buf[ptr] += A[i][7]*B[7][j], then acc_ptr++
    0xE483E3E6,  // [35] BNE  r3,  r9, j_loop   ; if j < 8, next column
    0xC1000201,  // [36] ADDI r2, r2, 1          ; i++
    0xE483E2E1,  // [37] BNE  r2, r9, i_loop     ; if i < 8, next row
    0x00000001,  // [38] HALT
};
#define bench_matmul_pp_LEN 39
