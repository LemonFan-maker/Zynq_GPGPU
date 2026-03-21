; bench_matmul.asm
; 8x8 Matrix Multiply: C = A * B
; A[8][8] at DMEM[0..63],   A[i][k] = DMEM[i*8 + k]
; B[8][8] at DMEM[64..127], B[k][j] = DMEM[64 + k*8 + j]
; C[8][8] at DMEM[128..191], C[i][j] = DMEM[128 + i*8 + j]
;
; All SIMD lanes compute the same result (broadcast).
; With register forwarding, no gap NOPs needed.
;
; Register allocation:
;   r0  = 0 (hardwired)
;   r1  = accumulator (MAC target)
;   r2  = i (outer loop counter, 0..7)
;   r3  = j (inner loop counter, 0..7)
;   r4  = row_base_A = i * 8
;   r5  = col_base_B = 64 + j
;   r6, r7   = ping-pong pair A (LDR temps for A values)
;   r8  = output pointer (starts at 128, incremented)
;   r9  = 8 (loop bound / row width)
;   r10 = 64 (B array base offset)
;   r21, r22 = ping-pong pair B (LDR temps for B values)

    ADDI r9,  r0, 8        ; loop bound = 8
    ADDI r10, r0, 64       ; B base offset
    ADDI r2,  r0, 0        ; i = 0
    ADDI r8,  r0, 128      ; output pointer = 128

i_loop:
    MUL  r4, r2, r9        ; row_base_A = i * 8
    ADDI r3, r0, 0         ; j = 0

j_loop:
    ADD  r5, r10, r3       ; col_base_B = 64 + j
    ADDI r1, r0, 0         ; acc = 0

    ; === Unrolled k=0..7 MAC kernel ===
    ; Prologue: load first A,B pair
    LDR  r6,  [r4 + 0]    ; A[i][0]
    LDR  r7,  [r5 + 0]    ; B[0][j]

    ; k=0 MAC + k=1 A load
    LDR  r21, [r4 + 1]    ; A[i][1]
    MAC  r1,  r6,  r7     ; acc += A[i][0]*B[0][j]

    ; k=1 B load + k=2 A load + k=1 MAC
    LDR  r22, [r5 + 8]    ; B[1][j]
    LDR  r6,  [r4 + 2]    ; A[i][2]
    MAC  r1,  r21, r22    ; acc += A[i][1]*B[1][j]

    ; k=2 B load + k=3 A load + k=2 MAC
    LDR  r7,  [r5 + 16]   ; B[2][j]
    LDR  r21, [r4 + 3]    ; A[i][3]
    MAC  r1,  r6,  r7     ; acc += A[i][2]*B[2][j]

    ; k=3 B load + k=4 A load + k=3 MAC
    LDR  r22, [r5 + 24]   ; B[3][j]
    LDR  r6,  [r4 + 4]    ; A[i][4]
    MAC  r1,  r21, r22    ; acc += A[i][3]*B[3][j]

    ; k=4 B load + k=5 A load + k=4 MAC
    LDR  r7,  [r5 + 32]   ; B[4][j]
    LDR  r21, [r4 + 5]    ; A[i][5]
    MAC  r1,  r6,  r7     ; acc += A[i][4]*B[4][j]

    ; k=5 B load + k=6 A load + k=5 MAC
    LDR  r22, [r5 + 40]   ; B[5][j]
    LDR  r6,  [r4 + 6]    ; A[i][6]
    MAC  r1,  r21, r22    ; acc += A[i][5]*B[5][j]

    ; k=6 B load + k=7 A load + k=6 MAC
    LDR  r7,  [r5 + 48]   ; B[6][j]
    LDR  r21, [r4 + 7]    ; A[i][7]
    MAC  r1,  r6,  r7     ; acc += A[i][6]*B[6][j]

    ; k=7 epilogue
    LDR  r22, [r5 + 56]   ; B[7][j]
    ADDI r3,  r3,  1      ; j++
    MAC  r1,  r21, r22    ; acc += A[i][7]*B[7][j]

    ; Store result
    STR  r1,  [r8 + 0]    ; C[i][j] = acc
    ADDI r8,  r8,  1      ; output pointer++
    BNE  r3,  r9, j_loop  ; if j < 8, next column

    ADDI r2, r2, 1         ; i++
    BNE  r2, r9, i_loop    ; if i < 8, next row

    HALT
