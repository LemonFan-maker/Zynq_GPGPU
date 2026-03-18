; mac_dot.asm
; A = {1,2,3,4} @ DMEM[100..103]
; B = {5,6,7,8} @ DMEM[104..107]
; result = 1*5 + 2*6 + 3*7 + 4*8 = 70 → DMEM[93]

    ADDI r1, r0, 0          ; sum = 0
    ADDI r2, r0, 0          ; i = 0
    ADDI r3, r0, 4          ; loop count
    ADDI r4, r0, 100        ; A base
    ADDI r5, r0, 104        ; B base

loop:
    LDR  r6, [r4 + 0]       ; A[i]
    LDR  r6, [r4 + 0]
    LDR  r7, [r5 + 0]       ; B[i]
    LDR  r7, [r5 + 0]
    NOP
    MAC  r1, r6, r7         ; sum += A[i]*B[i]
    ADDI r4, r4, 1
    ADDI r5, r5, 1
    ADDI r2, r2, 1
    NOP
    BNE  r2, r3, loop
    NOP
    STR  r1, [r0 + 93]
