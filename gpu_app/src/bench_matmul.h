// 由 gpuasm 从 bench_matmul.asm 之中生成
#include <stdint.h>

uint32_t bench_matmul[] = {
    0xC4800008,  // [0] ADDI r9,  r0, 8        ; loop bound = 8
    0xC5000040,  // [1] ADDI r10, r0, 64       ; B base offset
    0xC1000000,  // [2] ADDI r2,  r0, 0        ; i = 0
    0xC4000080,  // [3] ADDI r8,  r0, 128      ; output pointer = 128
    0x22012200,  // [4] MUL  r4, r2, r9        ; row_base_A = i * 8
    0xC1800000,  // [5] ADDI r3, r0, 0         ; j = 0
    0x02806A00,  // [6] ADD  r5, r10, r3       ; col_base_B = 64 + j
    0xC0800000,  // [7] ADDI r1, r0, 0         ; acc = 0
    0x83000400,  // [8] LDR  r6,  [r4 + 0]    ; A[i][0]
    0x83800500,  // [9] LDR  r7,  [r5 + 0]    ; B[0][j]
    0x8A800401,  // [10] LDR  r21, [r4 + 1]    ; A[i][1]
    0x2080E601,  // [11] MAC  r1,  r6,  r7     ; acc += A[i][0]*B[0][j]
    0x8B000508,  // [12] LDR  r22, [r5 + 8]    ; B[1][j]
    0x83000402,  // [13] LDR  r6,  [r4 + 2]    ; A[i][2]
    0x2082D501,  // [14] MAC  r1,  r21, r22    ; acc += A[i][1]*B[1][j]
    0x83800510,  // [15] LDR  r7,  [r5 + 16]   ; B[2][j]
    0x8A800403,  // [16] LDR  r21, [r4 + 3]    ; A[i][3]
    0x2080E601,  // [17] MAC  r1,  r6,  r7     ; acc += A[i][2]*B[2][j]
    0x8B000518,  // [18] LDR  r22, [r5 + 24]   ; B[3][j]
    0x83000404,  // [19] LDR  r6,  [r4 + 4]    ; A[i][4]
    0x2082D501,  // [20] MAC  r1,  r21, r22    ; acc += A[i][3]*B[3][j]
    0x83800520,  // [21] LDR  r7,  [r5 + 32]   ; B[4][j]
    0x8A800405,  // [22] LDR  r21, [r4 + 5]    ; A[i][5]
    0x2080E601,  // [23] MAC  r1,  r6,  r7     ; acc += A[i][4]*B[4][j]
    0x8B000528,  // [24] LDR  r22, [r5 + 40]   ; B[5][j]
    0x83000406,  // [25] LDR  r6,  [r4 + 6]    ; A[i][6]
    0x2082D501,  // [26] MAC  r1,  r21, r22    ; acc += A[i][5]*B[5][j]
    0x83800530,  // [27] LDR  r7,  [r5 + 48]   ; B[6][j]
    0x8A800407,  // [28] LDR  r21, [r4 + 7]    ; A[i][7]
    0x2080E601,  // [29] MAC  r1,  r6,  r7     ; acc += A[i][6]*B[6][j]
    0x8B000538,  // [30] LDR  r22, [r5 + 56]   ; B[7][j]
    0xC1800301,  // [31] ADDI r3,  r3,  1      ; j++
    0x2082D501,  // [32] MAC  r1,  r21, r22    ; acc += A[i][7]*B[7][j]
    0x90002800,  // [33] STR  r1,  [r8 + 0]    ; C[i][j] = acc
    0xC4000801,  // [34] ADDI r8,  r8,  1      ; output pointer++
    0xE483E3E3,  // [35] BNE  r3,  r9, j_loop  ; if j < 8, next column
    0xC1000201,  // [36] ADDI r2, r2, 1         ; i++
    0xE483E2DF,  // [37] BNE  r2, r9, i_loop    ; if i < 8, next row
    0x00000001,  // [38] HALT
};
#define bench_matmul_LEN 39
