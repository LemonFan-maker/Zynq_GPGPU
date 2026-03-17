`timescale 1ns / 1ps
import gpu_types_pkg::*;

module tb_gpu_decoder();

    logic [31:0] tb_instruction;
    
    logic        tb_out_we;
    logic [4:0]  tb_out_rd_addr;
    logic [4:0]  tb_out_rs1_addr;
    logic [4:0]  tb_out_rs2_addr;
    alu_op_t     tb_out_alu_op;
    logic [31:0] tb_out_imm;
    logic        tb_out_mem_re;
    logic        tb_out_mem_we;

    gpu_decoder uut (
        .instruction  (tb_instruction),
        .out_we       (tb_out_we),
        .out_rd_addr  (tb_out_rd_addr),
        .out_rs1_addr (tb_out_rs1_addr),
        .out_rs2_addr (tb_out_rs2_addr),
        .out_alu_op   (tb_out_alu_op),
        .out_imm      (tb_out_imm),
        .out_mem_re   (tb_out_mem_re),
        .out_mem_we   (tb_out_mem_we)
    );

    initial begin
        // 测试常规ADD指令
        tb_instruction = 32'h01844000; 
        #10;
        
        // 测试LDR读内存指令
        tb_instruction = 32'h8284000A; // LDR r5, [r2 + 10]
        #10;
        
        // 测试STR写内存指令
        tb_instruction = 32'h900CDFFC; // STR r6, [r3 - 4]
        #10;
        
        $display("V2 Decoder Simulation Finished!");
        $finish;
    end

endmodule