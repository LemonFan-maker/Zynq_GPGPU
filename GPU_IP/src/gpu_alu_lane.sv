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

    (* use_dsp = "yes" *) logic [31:0] mul_result;
    assign mul_result = operand_a * operand_b;

    always_comb begin
        case (alu_op)
            ALU_ADD: result = operand_a + operand_b;
            ALU_SUB: result = operand_a - operand_b;
            ALU_MUL: result = mul_result;
            ALU_AND: result = operand_a & operand_b;
            ALU_OR:  result = operand_a | operand_b;
            ALU_XOR: result = operand_a ^ operand_b;
            ALU_SLL: result = operand_a << operand_b[4:0];
            ALU_SRL: result = operand_a >> operand_b[4:0];
            ALU_SLT: result = {31'b0, $signed(operand_a) < $signed(operand_b)};
            OP_LDR:  result = operand_a + operand_b;
            OP_STR:  result = operand_a + operand_b;
            OP_SETM: result = operand_a - operand_b;
            OP_ADDI: result = operand_a + operand_b;
            OP_BEQ:  result = operand_a - operand_b;
            OP_BNE:  result = operand_a - operand_b;
            default: result = 32'b0;
        endcase
    end

    assign flag_zero = (result == 32'h0);

endmodule
