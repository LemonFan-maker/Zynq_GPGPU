`timescale 1ns / 1ps
import gpu_types_pkg::*;

module gpu_alu_lane (
    input  logic        clk,
    input  logic        rst_n,
    input  alu_op_t     alu_op,
    input  logic [31:0] operand_a,
    input  logic [31:0] operand_b,
    output logic [31:0] result,
    output logic        flag_zero
);

    always_comb begin
        case (alu_op)
            ALU_ADD: result = operand_a + operand_b;
            ALU_SUB: result = operand_a - operand_b;
            ALU_AND: result = operand_a & operand_b;
            ALU_OR:  result = operand_a | operand_b;
            ALU_XOR: result = operand_a ^ operand_b;
            OP_LDR:  result = operand_a + operand_b; 
            OP_STR:  result = operand_a + operand_b; 
            
            // 处理SETM指令，相减判断是否相等
            OP_SETM: result = operand_a - operand_b; 
            
            default: result = 32'b0;
        endcase
    end

    // 如果结果为0，则flag_zero为1
    assign flag_zero = (result == 32'h0);

endmodule