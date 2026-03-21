; bench_matmul_pp_ovr.asm
; Parameterized 8x8 MatMul with Hardware Accumulator Buffer (first k-split kernel)
; A[8][8] at DMEM[base..base+63]
; B[8][8] at DMEM[base+64..base+127]
; First multiply per output element uses MUL_OVR to avoid explicit ACC clear.
; Subsequent k-splits should use bench_matmul_pp.asm (MAC_ACC-only kernel).
; base = DMEM[250] (set by ARM before launch, 0 for ping, 512 for pong)
;
; Register allocation:
;   r0  = 0 (hardwired)
;   r2  = i (0..7)
;   r3  = j (0..7)
;   r4  = A row base (base + i*8)
;   r5  = B col base (B_base + j)
;   r6, r7   = ping-pong A temps
;   r9  = 8 (loop bound)
;   r10 = B_base (base + 64)
;   r21, r22 = ping-pong B temps
;   r23 = base address (loaded from DMEM[250])

    ADDI r9,  r0, 8         ; loop bound = 8
    ADDI r23, r0, 250       ; parameter address
    LDR  r23, [r23 + 0]     ; base = DMEM[250]
    ADD  r10, r23, r0       ; r10 = base
    ADDI r10, r10, 64       ; B_base = base + 64
    ADDI r2,  r0, 0         ; i = 0

i_loop:
    MUL  r4, r2, r9         ; row offset = i * 8
    ADD  r4, r4, r23        ; A_row_base = base + i*8
    ADDI r3, r0, 0          ; j = 0

j_loop:
    ADD  r5, r10, r3        ; col_base_B = B_base + j

    ; === Unrolled k=0..7 kernel ===
    LDR  r6,  [r4 + 0]     ; A[i][0]
    LDR  r7,  [r5 + 0]     ; B[0][j]
    LDR  r21, [r4 + 1]     ; A[i][1]
    MUL_OVR r6,  r7         ; acc_buf[ptr] =  A[i][0]*B[0][j]
    LDR  r22, [r5 + 8]     ; B[1][j]
    LDR  r6,  [r4 + 2]     ; A[i][2]
    MAC_ACC r21, r22        ; acc_buf[ptr] += A[i][1]*B[1][j]
    LDR  r7,  [r5 + 16]    ; B[2][j]
    LDR  r21, [r4 + 3]     ; A[i][3]
    MAC_ACC r6,  r7         ; acc_buf[ptr] += A[i][2]*B[2][j]
    LDR  r22, [r5 + 24]    ; B[3][j]
    LDR  r6,  [r4 + 4]     ; A[i][4]
    MAC_ACC r21, r22        ; acc_buf[ptr] += A[i][3]*B[3][j]
    LDR  r7,  [r5 + 32]    ; B[4][j]
    LDR  r21, [r4 + 5]     ; A[i][5]
    MAC_ACC r6,  r7         ; acc_buf[ptr] += A[i][4]*B[4][j]
    LDR  r22, [r5 + 40]    ; B[5][j]
    LDR  r6,  [r4 + 6]     ; A[i][6]
    MAC_ACC r21, r22        ; acc_buf[ptr] += A[i][5]*B[5][j]
    LDR  r7,  [r5 + 48]    ; B[6][j]
    LDR  r21, [r4 + 7]     ; A[i][7]
    MAC_ACC r6,  r7         ; acc_buf[ptr] += A[i][6]*B[6][j]
    LDR  r22, [r5 + 56]    ; B[7][j]
    ADDI r3,  r3,  1       ; j++
    MAC_ACC r21, r22        ; acc_buf[ptr] += A[i][7]*B[7][j]

    ACC_NEXT                ; acc_ptr++ (move to next output element)
    BNE  r3,  r9, j_loop   ; if j < 8, next column

    ADDI r2, r2, 1          ; i++
    BNE  r2, r9, i_loop     ; if i < 8, next row

    HALT
