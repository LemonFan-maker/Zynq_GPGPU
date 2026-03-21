; bench_vecadd.asm
; Vector ADD: DMEM[512+i] = DMEM[0+i] + DMEM[256+i], for i=0..255
; Measures raw ALU + LDR/STR throughput over 256 entries
;
; Register allocation:
;   r1 = value A
;   r2 = addr_B / value B
;   r3 = result
;   r4 = index i
;   r5 = 256 (loop bound)
;   r6 = 256 (base offset for B array)
;   r7 = output address
;
; With register forwarding, no gap NOPs needed.

    ADDI r5, r0, 256     ; loop bound
    ADDI r6, r0, 256     ; B base offset
    ADDI r4, r0, 0       ; i = 0

loop:
    LDR  r1, [r4 + 0]    ; A[i] = DMEM[i]
    ADD  r2, r4, r6      ; addr_B = i + 256
    ADD  r7, r4, r0      ; save i for output addr
    LDR  r2, [r2 + 0]    ; B[i] = DMEM[i+256]
    ADDI r7, r7, 512     ; out_addr = saved_i + 512
    ADD  r3, r1, r2      ; result = A[i] + B[i]
    ADDI r4, r4, 1       ; i++
    STR  r3, [r7 + 0]    ; DMEM[saved_i+512] = result
    BNE  r4, r5, loop    ; if i < 256 continue

    HALT
