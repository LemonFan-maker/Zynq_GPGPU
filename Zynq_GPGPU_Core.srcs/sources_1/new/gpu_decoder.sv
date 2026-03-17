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
    output logic        out_mem_we
);

    // 解析指令字段
    logic [3:0] opcode;
    assign opcode       = instruction[31:28];
    assign out_rd_addr  = instruction[27:23];
    assign out_rs2_addr = instruction[17:13];
    assign out_rs1_addr = instruction[12:8];
    
    // 立即数符号扩展
    assign out_imm      = {{24{instruction[7]}}, instruction[7:0]};

    // 生成控制信号
    always_comb begin
        // 防止锁存器产生
        out_we     = 1'b0;
        out_mem_re = 1'b0;
        out_mem_we = 1'b0;
        out_alu_op = ALU_ADD;

        case (opcode)
            4'h0: begin
                out_alu_op = ALU_ADD;
                out_we     = 1'b1;
            end
            
            4'h8: begin // LDR: Load Register
                out_alu_op = OP_LDR;
                out_we     = 1'b1;
                out_mem_re = 1'b1;
            end
            
            4'h9: begin // STR: Store Register
                out_alu_op = OP_STR;
                out_mem_we = 1'b1;
            end
            
            4'hA: begin 
                out_alu_op = OP_SETM;
                out_we     = 1'b0; // 掩码指令不写回通用寄存器
            end
            
            default: begin
                out_alu_op = ALU_ADD;
            end
        endcase
    end

endmodule