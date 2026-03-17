`timescale 1ns / 1ps
import gpu_types_pkg::*;

module gpu_core_top (
    input  logic        clk,
    input  logic        rst_n,
    
    output logic [31:0] out_imem_addr,
    input  logic [31:0] in_imem_data,

    output logic        out_dmem_re,
    output logic [3:0]  out_dmem_we,
    output logic [31:0] out_dmem_addr, 
    output logic [127:0] out_dmem_wdata,
    input  logic [127:0] in_dmem_rdata,

    output logic [3:0]  out_flag_zero
);

    logic [31:0] pc;
    assign out_imem_addr = pc;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            pc <= 32'h0;
        end else begin
            pc <= pc + 1'b1;
        end
    end

    logic        dec_we;
    logic [4:0]  dec_rd_addr;
    logic [4:0]  dec_rs1_addr;
    logic [4:0]  dec_rs2_addr;
    alu_op_t     dec_alu_op;
    logic [31:0] dec_imm;
    logic        dec_mem_re;
    logic        dec_mem_we;

    gpu_decoder u_decoder (
        .instruction  (in_imem_data),
        .out_we       (dec_we),
        .out_rd_addr  (dec_rd_addr),
        .out_rs1_addr (dec_rs1_addr),
        .out_rs2_addr (dec_rs2_addr),
        .out_alu_op   (dec_alu_op),
        .out_imm      (dec_imm),
        .out_mem_re   (dec_mem_re),
        .out_mem_we   (dec_mem_we)
    );

    gpu_exec_unit #( .NUM_LANES(4) ) u_exec_unit (
        .clk            (clk),
        .rst_n          (rst_n),
        .in_we          (dec_we),
        .in_rd_addr     (dec_rd_addr),
        .in_rs1_addr    (dec_rs1_addr),
        .in_rs2_addr    (dec_rs2_addr),
        .in_alu_op      (dec_alu_op),
        .in_imm         (dec_imm),
        .in_mem_re      (dec_mem_re),
        .in_mem_we      (dec_mem_we),
        
        // 外部接口映射
        .out_dmem_re    (out_dmem_re),
        .out_dmem_we    (out_dmem_we),    // 内部输出4位
        .out_dmem_addr  (out_dmem_addr),
        .out_dmem_wdata (out_dmem_wdata), // 内部输出128位
        .in_dmem_rdata  (in_dmem_rdata),  // 内部接收128位
        
        .out_flag_zero  (out_flag_zero)
    );

endmodule