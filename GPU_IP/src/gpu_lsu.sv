`timescale 1ns / 1ps
import gpu_types_pkg::*;

module gpu_lsu #(
    parameter int NUM_LANES = 4
)(
    input  logic        clk,
    input  logic        rst_n,

    input  logic        in_mem_re,
    input  logic        in_mem_we,
    input  logic [31:0] in_base_addr,
    input  logic [31:0] in_offset,

    input  logic [NUM_LANES*32-1:0] in_wdata_vector,

    output logic        out_dmem_re,
    output logic        out_dmem_we,
    output logic [31:0] out_dmem_addr,
    output logic [NUM_LANES*32-1:0] out_dmem_wdata,
    input  logic [NUM_LANES*32-1:0] in_dmem_rdata,

    // 分发给所有Lane的读回数据
    output logic [NUM_LANES*32-1:0] out_wb_data_vector
);

    assign out_dmem_addr = in_base_addr + in_offset;

    // 读写控制及128-bit宽带数据透传
    assign out_dmem_re    = in_mem_re;
    assign out_dmem_we    = in_mem_we;
    assign out_dmem_wdata = in_wdata_vector;

    // 读回的128-bit数据直接分发
    assign out_wb_data_vector = in_dmem_rdata;

endmodule