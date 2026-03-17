`timescale 1ns / 1ps
import gpu_types_pkg::*;

module gpu_core_top #(
    parameter NUM_LANES = 4
)(
    input  logic        clk,
    input  logic        rst_n,
    
    // 指令内存接口
    output logic [31:0] out_imem_addr,
    input  logic [31:0] in_imem_data,
    
    // 数据内存接口
    output logic        out_dmem_re,
    output logic [NUM_LANES-1:0]  out_dmem_we,    // 16个写使能位
    output logic [31:0] out_dmem_addr,            // 标量基地址
    output logic [(NUM_LANES*32)-1:0] out_dmem_wdata, // 512位宽写数据
    input  logic [(NUM_LANES*32)-1:0] in_dmem_rdata,  // 512位宽读数据
    
    output logic [NUM_LANES-1:0]  out_flag_zero   // 16 个状态标志位
);

    // 内部总线连线
    logic        we;
    logic [4:0]  rd_addr;
    logic [4:0]  rs1_addr;
    logic [4:0]  rs2_addr;
    alu_op_t     alu_op;
    logic [31:0] imm;
    logic        mem_re;
    logic        mem_we;

    // 标量取指译码单元
    logic [31:0] pc;
    assign out_imem_addr = pc;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            pc <= 32'h0;
        end else begin
            pc <= pc + 1'b1;
        end
    end

    gpu_decoder u_decoder (
        .instruction  (in_imem_data),
        .out_we       (we),
        .out_rd_addr  (rd_addr),
        .out_rs1_addr (rs1_addr),
        .out_rs2_addr (rs2_addr),
        .out_alu_op   (alu_op),
        .out_imm      (imm),
        .out_mem_re   (mem_re),
        .out_mem_we   (mem_we)
    );

    // 并行向量执行单元
    gpu_exec_unit #(
        .NUM_LANES (NUM_LANES)
    ) u_exec_unit (
        .clk            (clk),
        .rst_n          (rst_n),
        .in_we          (we),
        .in_rd_addr     (rd_addr),
        .in_rs1_addr    (rs1_addr),
        .in_rs2_addr    (rs2_addr),
        .in_alu_op      (alu_op),
        .in_imm         (imm),
        .in_mem_re      (mem_re),
        .in_mem_we      (mem_we),
        .out_dmem_re    (out_dmem_re),
        .out_dmem_we    (out_dmem_we),
        .out_dmem_addr  (out_dmem_addr),
        .out_dmem_wdata (out_dmem_wdata),
        .in_dmem_rdata  (in_dmem_rdata),
        .out_flag_zero  (out_flag_zero)
    );

endmodule