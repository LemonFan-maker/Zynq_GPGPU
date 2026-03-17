`timescale 1ns / 1ps
import gpu_types_pkg::*;

module gpu_exec_unit #(
    parameter int NUM_LANES = 4
)(
    input  logic        clk,
    input  logic        rst_n,
    input  logic        in_we,
    input  logic [4:0]  in_rd_addr,
    input  logic [4:0]  in_rs1_addr,
    input  logic [4:0]  in_rs2_addr,
    input  alu_op_t     in_alu_op,
    input  logic [31:0] in_imm,
    input  logic        in_mem_re,
    input  logic        in_mem_we,

    output logic        out_dmem_re,
    output logic [NUM_LANES-1:0] out_dmem_we, 
    output logic [31:0] out_dmem_addr,
    output logic [NUM_LANES*32-1:0] out_dmem_wdata,
    input  logic [NUM_LANES*32-1:0] in_dmem_rdata,

    output logic [NUM_LANES-1:0] out_flag_zero
);

    logic       we_pipe;
    logic [4:0] rd_addr_pipe;
    logic       wb_sel_pipe;

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            we_pipe <= 0; rd_addr_pipe <= 0; wb_sel_pipe <= 0;
        end else begin
            we_pipe <= in_we; rd_addr_pipe <= in_rd_addr; wb_sel_pipe <= in_mem_re;
        end
    end

    logic [NUM_LANES-1:0] exec_mask;
    
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            exec_mask <= 4'b1111;
        end else if (in_alu_op == OP_SETM) begin
            // 当执行SETM指令时，如果ALU判断rs1==rs2(flag_zero为1)，
            exec_mask <= out_flag_zero; 
        end
    end

    logic [31:0] lane_rdata1 [NUM_LANES];
    logic [31:0] lane_rdata2 [NUM_LANES];
    logic [31:0] lane_alu_res [NUM_LANES];
    logic [31:0] lane_lsu_res [NUM_LANES];

    logic [NUM_LANES*32-1:0] lsu_wdata_packed;
    logic [NUM_LANES*32-1:0] lsu_rdata_packed;

    genvar i;
    generate
        for (i = 0; i < NUM_LANES; i = i + 1) begin : gen_lanes
            assign lane_lsu_res[i] = lsu_rdata_packed[i*32 +: 32];
            assign lsu_wdata_packed[i*32 +: 32] = lane_rdata2[i];
            
            assign out_dmem_we[i] = in_mem_we & exec_mask[i];

            logic [31:0] final_wb;
            assign final_wb = (wb_sel_pipe) ? lane_lsu_res[i] : lane_alu_res[i];

            gpu_regfile u_rf (
                .clk(clk), .we(we_pipe & exec_mask[i]), 
                .waddr(rd_addr_pipe), .wdata(final_wb),
                .raddr1(in_rs1_addr), .rdata1(lane_rdata1[i]),
                .raddr2(in_rs2_addr), .rdata2(lane_rdata2[i])
            );

            gpu_alu_lane u_alu (
                .clk(clk), .rst_n(rst_n), .alu_op(in_alu_op),
                .operand_a(lane_rdata1[i]), .operand_b(lane_rdata2[i]),
                .result(lane_alu_res[i]), .flag_zero(out_flag_zero[i])
            );
        end
    endgenerate

    gpu_lsu #( .NUM_LANES(NUM_LANES) ) u_vector_lsu (
        .clk(clk), .rst_n(rst_n),
        .in_mem_re(in_mem_re), .in_mem_we(in_mem_we),
        .in_base_addr(lane_rdata1[0]), 
        .in_offset(in_imm),
        .in_wdata_vector(lsu_wdata_packed),
        .out_dmem_re(out_dmem_re), 
        .out_dmem_addr(out_dmem_addr), .out_dmem_wdata(out_dmem_wdata),
        .in_dmem_rdata(in_dmem_rdata),
        .out_wb_data_vector(lsu_rdata_packed)
    );

endmodule