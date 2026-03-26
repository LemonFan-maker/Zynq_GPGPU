; maxpool_2x2.asm
; Performs 2x2 max pooling with stride=2.
; Operates on HWC format.
; Expects parameters initialized by host:
;   r1  = Input base address in DMEM
;   r2  = Output base address in DMEM
;   r3  = Output Height (OH)
;   r4  = Output Width (OW)
;   r5  = Channels (C)  <- assumed to be padded to multiple of 4 if needed, but works for any
;   r6  = Input Width offset (IW * C)

    ADDI r0, r0, 0        ; dummy

    ; Wait, we need to generate maxpool inline from C because the parameters (OH, OW, C) differ per layer
    ; So I will instead just implement it as an inline generator in C like the MatMul DP4A ones.
    HALT
