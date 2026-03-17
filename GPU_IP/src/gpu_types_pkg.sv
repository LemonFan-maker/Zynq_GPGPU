`timescale 1ns / 1ps

package gpu_types_pkg;

    typedef enum logic [3:0] {
        ALU_ADD  = 4'h0,
        ALU_SUB  = 4'h1,
        ALU_MUL  = 4'h2,
        ALU_AND  = 4'h3,
        ALU_OR   = 4'h4,
        ALU_XOR  = 4'h5,
        ALU_SLL  = 4'h6,
        ALU_SRL  = 4'h7,
        OP_LDR   = 4'h8,
        OP_STR   = 4'h9,
        OP_SETM  = 4'hA,
        ALU_SLT  = 4'hB,
        OP_ADDI  = 4'hC,
        OP_BEQ   = 4'hD,
        OP_BNE   = 4'hE,
        OP_JMP   = 4'hF
    } alu_op_t;

endpackage