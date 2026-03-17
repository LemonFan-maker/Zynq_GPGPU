`timescale 1ns / 1ps

package gpu_types_pkg;

    typedef enum logic [3:0] {
        // 算术与逻辑运算
        ALU_ADD  = 4'b0000,
        ALU_SUB  = 4'b0001,
        ALU_AND  = 4'b0010,
        ALU_OR   = 4'b0011,
        ALU_XOR  = 4'b0100,
        
        OP_LDR   = 4'b1000, 
        OP_STR   = 4'b1001, 
        
        OP_SETM  = 4'b1010  
    } alu_op_t;

endpackage