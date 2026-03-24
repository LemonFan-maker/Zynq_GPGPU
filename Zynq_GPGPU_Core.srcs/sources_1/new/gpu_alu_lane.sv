`timescale 1ns / 1ps
import gpu_types_pkg::*;

module gpu_alu_lane (
    input  logic        clk,
    input  logic        rst_n,
    input  alu_op_t     alu_op,
    input  logic        in_is_dp4a,
    input  logic [31:0] operand_a,
    input  logic [31:0] operand_b,
    output logic [31:0] result,
    output logic        flag_zero
);

    logic signed [7:0] a0, a1, a2, a3;
    logic signed [7:0] b0, b1, b2, b3;
    logic signed [15:0] p0, p1, p2, p3;
    logic signed [17:0] dp4a_sum;

    assign a0 = operand_a[7:0];
    assign a1 = operand_a[15:8];
    assign a2 = operand_a[23:16];
    assign a3 = operand_a[31:24];

    assign b0 = operand_b[7:0];
    assign b1 = operand_b[15:8];
    assign b2 = operand_b[23:16];
    assign b3 = operand_b[31:24];

    assign p0 = a0 * b0;
    assign p1 = a1 * b1;
    assign p2 = a2 * b2;
    assign p3 = a3 * b3;
    assign dp4a_sum = p0 + p1 + p2 + p3;

    (* use_dsp = "yes" *) logic [31:0] mul_result;
    assign mul_result = in_is_dp4a ? {{14{dp4a_sum[17]}}, dp4a_sum} : (operand_a * operand_b);

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

    assign flag_zero = (alu_op == OP_SETM) ? (operand_a == operand_b)
                                           : (result == 32'h0);

endmodule
