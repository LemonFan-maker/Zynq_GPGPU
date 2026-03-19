// 由 gpuasm 从 conv2d_3x3.asm 之中生成
#include <stdint.h>

uint32_t conv2d_3x3[] = {
    0xC5800001,  // [0] ADDI r11, r0, 1       ; k[0][0] = 1
    0xC6000002,  // [1] ADDI r12, r0, 2       ; k[0][1] = 2
    0xC6800003,  // [2] ADDI r13, r0, 3       ; k[0][2] = 3
    0xC7000004,  // [3] ADDI r14, r0, 4       ; k[1][0] = 4
    0xC7800005,  // [4] ADDI r15, r0, 5       ; k[1][1] = 5
    0xC8000006,  // [5] ADDI r16, r0, 6       ; k[1][2] = 6
    0xC8800007,  // [6] ADDI r17, r0, 7       ; k[2][0] = 7
    0xC9000008,  // [7] ADDI r18, r0, 8       ; k[2][1] = 8
    0xC9800009,  // [8] ADDI r19, r0, 9       ; k[2][2] = 9
    0xC4800005,  // [9] ADDI r9,  r0, 5       ; ROW_WIDTH = 5
    0xCA000003,  // [10] ADDI r20, r0, 3       ; LOOP_BOUND = 3
    0xC4000032,  // [11] ADDI r8,  r0, 50      ; output pointer -> DMEM[50]
    0xC1000000,  // [12] ADDI r2,  r0, 0       ; oy = 0
    0x21812200,  // [13] MUL  r3, r2, r9       ; row_base = oy * 5
    0xC2800000,  // [14] ADDI r5, r0, 0        ; ox = 0 (fills MUL delay slot)
    0x00000000,  // [15] NOP                    ; delay for ADDI r5 writeback
    0x0200A300,  // [16] ADD  r4, r3, r5       ; pixel_base = row_base + ox
    0xC0800000,  // [17] ADDI r1, r0, 0        ; acc = 0 (fills ADD delay slot)
    0x83000400,  // [18] LDR  r6, [r4 + 0]     ; in[oy+0][ox+0]
    0x83800401,  // [19] LDR  r7, [r4 + 1]     ; in[oy+0][ox+1]
    0x20816601,  // [20] MAC  r1, r6, r11      ; acc += in[0,0] * k[0,0]
    0x83000402,  // [21] LDR  r6, [r4 + 2]     ; in[oy+0][ox+2]
    0x20818701,  // [22] MAC  r1, r7, r12      ; acc += in[0,1] * k[0,1]
    0x83800405,  // [23] LDR  r7, [r4 + 5]     ; in[oy+1][ox+0]
    0x2081A601,  // [24] MAC  r1, r6, r13      ; acc += in[0,2] * k[0,2]
    0x83000406,  // [25] LDR  r6, [r4 + 6]     ; in[oy+1][ox+1]
    0x2081C701,  // [26] MAC  r1, r7, r14      ; acc += in[1,0] * k[1,0]
    0x83800407,  // [27] LDR  r7, [r4 + 7]     ; in[oy+1][ox+2]
    0x2081E601,  // [28] MAC  r1, r6, r15      ; acc += in[1,1] * k[1,1]
    0x8300040A,  // [29] LDR  r6, [r4 + 10]    ; in[oy+2][ox+0]
    0x20820701,  // [30] MAC  r1, r7, r16      ; acc += in[1,2] * k[1,2]
    0x8380040B,  // [31] LDR  r7, [r4 + 11]    ; in[oy+2][ox+1]
    0x20822601,  // [32] MAC  r1, r6, r17      ; acc += in[2,0] * k[2,0]
    0x8300040C,  // [33] LDR  r6, [r4 + 12]    ; in[oy+2][ox+2]
    0x20824701,  // [34] MAC  r1, r7, r18      ; acc += in[2,1] * k[2,1]
    0xC2800501,  // [35] ADDI r5, r5, 1        ; ox++ (fills LDR r6 delay slot)
    0x20826601,  // [36] MAC  r1, r6, r19      ; acc += in[2,2] * k[2,2]
    0x00000000,  // [37] NOP                    ; delay for MAC r1 result
    0x90002800,  // [38] STR  r1, [r8 + 0]     ; output[oy][ox] = acc
    0xC4000801,  // [39] ADDI r8, r8, 1        ; output pointer++
    0xEA03E5E8,  // [40] BNE  r5, r20, ox_loop ; if ox < 3, next column
    0xC1000201,  // [41] ADDI r2, r2, 1        ; oy++
    0x00000000,  // [42] NOP                    ; delay for ADDI r2 result
    0xEA03E2E2,  // [43] BNE  r2, r20, oy_loop ; if oy < 3, next row
    0x00000001,  // [44] HALT                   ; done — signal completion
};
#define conv2d_3x3_LEN 45
