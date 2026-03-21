; conv2d_3x3.asm
; Register allocation:
;   r0  = 0 (hardwired)
;   r1  = accumulator
;   r2  = oy (outer loop counter)
;   r3  = row_base (oy * 5)
;   r4  = pixel_base (row_base + ox)
;   r5  = ox (inner loop counter)
;   r6/r7 = ping-pong temp registers for LDR/MAC interleaving
;   r8  = output pointer (starts at 50)
;   r9  = 5 (row width constant)
;   r11-r19 = kernel weights 1-9
;   r20 = 3 (loop bound)
;
; With register forwarding, no gap NOPs needed.

    ADDI r11, r0, 1       ; k[0][0] = 1
    ADDI r12, r0, 2       ; k[0][1] = 2
    ADDI r13, r0, 3       ; k[0][2] = 3
    ADDI r14, r0, 4       ; k[1][0] = 4
    ADDI r15, r0, 5       ; k[1][1] = 5
    ADDI r16, r0, 6       ; k[1][2] = 6
    ADDI r17, r0, 7       ; k[2][0] = 7
    ADDI r18, r0, 8       ; k[2][1] = 8
    ADDI r19, r0, 9       ; k[2][2] = 9

    ADDI r9,  r0, 5       ; ROW_WIDTH = 5
    ADDI r20, r0, 3       ; LOOP_BOUND = 3
    ADDI r2,  r0, 0       ; oy = 0
    ADDI r8,  r0, 50      ; output pointer -> DMEM[50]

oy_loop:
    MUL  r3, r2, r9       ; row_base = oy * 5
    ADDI r5, r0, 0        ; ox = 0

ox_loop:
    ADD  r4, r3, r5       ; pixel_base = row_base + ox
    ADDI r1, r0, 0        ; acc = 0

    ; === 9-MAC kernel ===
    LDR  r6, [r4 + 0]     ; in[oy+0][ox+0]
    LDR  r7, [r4 + 1]     ; in[oy+0][ox+1]
    MAC  r1, r6, r11      ; acc += in[0,0] * k[0,0]
    LDR  r6, [r4 + 2]     ; in[oy+0][ox+2]
    MAC  r1, r7, r12      ; acc += in[0,1] * k[0,1]
    LDR  r7, [r4 + 5]     ; in[oy+1][ox+0]
    MAC  r1, r6, r13      ; acc += in[0,2] * k[0,2]
    LDR  r6, [r4 + 6]     ; in[oy+1][ox+1]
    MAC  r1, r7, r14      ; acc += in[1,0] * k[1,0]
    LDR  r7, [r4 + 7]     ; in[oy+1][ox+2]
    MAC  r1, r6, r15      ; acc += in[1,1] * k[1,1]
    LDR  r6, [r4 + 10]    ; in[oy+2][ox+0]
    MAC  r1, r7, r16      ; acc += in[1,2] * k[1,2]
    LDR  r7, [r4 + 11]    ; in[oy+2][ox+1]
    MAC  r1, r6, r17      ; acc += in[2,0] * k[2,0]
    LDR  r6, [r4 + 12]    ; in[oy+2][ox+2]
    MAC  r1, r7, r18      ; acc += in[2,1] * k[2,1]
    ADDI r5, r5, 1        ; ox++
    MAC  r1, r6, r19      ; acc += in[2,2] * k[2,2]

    STR  r1, [r8 + 0]     ; output[oy][ox] = acc
    ADDI r8, r8, 1        ; output pointer++
    BNE  r5, r20, ox_loop ; if ox < 3, next column

    ADDI r2, r2, 1        ; oy++
    BNE  r2, r20, oy_loop ; if oy < 3, next row

    HALT                   ; done — signal completion
