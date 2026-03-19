`timescale 1ns / 1ps
import gpu_types_pkg::*;

module gpu_decoder (
    input  logic [31:0] instruction,

    output logic        out_we,
    output logic [4:0]  out_rd_addr,
    output logic [4:0]  out_rs1_addr,
    output logic [4:0]  out_rs2_addr,
    output alu_op_t     out_alu_op,
    output logic [31:0] out_imm,
    output logic        out_mem_re,
    output logic        out_mem_we,
    output logic        out_is_branch,
    output logic        out_is_jump,
    output logic        out_is_addi,
    output logic        out_is_mac,
    output logic        out_halt
);

    // HALT detection: instruction == 0x00000001
    assign out_halt = (instruction == 32'h00000001);

    logic [3:0] opcode;
    assign opcode       = instruction[31:28];
    assign out_rd_addr  = instruction[27:23];
    assign out_rs1_addr = instruction[12:8];

    // rs2: 普通指令用 [17:13]，BEQ/BNE 借用 rd 字段 [27:23] 做比较源
    assign out_rs2_addr = (opcode == 4'hD || opcode == 4'hE) ?
                          instruction[27:23] : instruction[17:13];

    // imm8: 8-bit 符号扩展 (用于 LDR/STR)
    logic [31:0] imm8_sext;
    assign imm8_sext = {{24{instruction[7]}}, instruction[7:0]};

    // imm13: {[17:13], [7:0]} 符号扩展 (用于 ADDI/BEQ/BNE/JMP)
    logic [12:0] imm13_raw;
    assign imm13_raw = {instruction[17:13], instruction[7:0]};
    logic [31:0] imm13_sext;
    assign imm13_sext = {{19{imm13_raw[12]}}, imm13_raw};

    // 根据指令类型选择立即数
    assign out_imm = (opcode >= 4'hC) ? imm13_sext : imm8_sext;

    always_comb begin
        out_we        = 1'b0;
        out_mem_re    = 1'b0;
        out_mem_we    = 1'b0;
        out_is_branch = 1'b0;
        out_is_jump   = 1'b0;
        out_is_addi   = 1'b0;
        out_is_mac    = 1'b0;
        out_alu_op    = ALU_ADD;

        case (opcode)
            4'h0: begin // ADD
                out_alu_op = ALU_ADD;
                out_we     = !out_halt;
            end
            4'h1: begin // SUB
                out_alu_op = ALU_SUB;
                out_we     = 1'b1;
            end
            4'h2: begin // MUL (imm8[0]=0) or MAC (imm8[0]=1)
                out_alu_op = ALU_MUL;
                out_we     = 1'b1;
                out_is_mac = instruction[0];
            end
            4'h3: begin // AND
                out_alu_op = ALU_AND;
                out_we     = 1'b1;
            end
            4'h4: begin // OR
                out_alu_op = ALU_OR;
                out_we     = 1'b1;
            end
            4'h5: begin // XOR
                out_alu_op = ALU_XOR;
                out_we     = 1'b1;
            end
            4'h6: begin // SLL
                out_alu_op = ALU_SLL;
                out_we     = 1'b1;
            end
            4'h7: begin // SRL
                out_alu_op = ALU_SRL;
                out_we     = 1'b1;
            end
            4'h8: begin // LDR
                out_alu_op = OP_LDR;
                out_we     = 1'b1;
                out_mem_re = 1'b1;
            end
            4'h9: begin // STR
                out_alu_op = OP_STR;
                out_mem_we = 1'b1;
            end
            4'hA: begin // SETM
                out_alu_op = OP_SETM;
            end
            4'hB: begin // SLT
                out_alu_op = ALU_SLT;
                out_we     = 1'b1;
            end
            4'hC: begin // ADDI
                out_alu_op  = OP_ADDI;
                out_we      = 1'b1;
                out_is_addi = 1'b1;
            end
            4'hD: begin // BEQ
                out_alu_op    = OP_BEQ;
                out_is_branch = 1'b1;
            end
            4'hE: begin // BNE
                out_alu_op    = OP_BNE;
                out_is_branch = 1'b1;
            end
            4'hF: begin // JMP
                out_alu_op  = OP_JMP;
                out_is_jump = 1'b1;
            end
        endcase
    end

endmodule
