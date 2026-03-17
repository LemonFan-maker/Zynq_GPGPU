`timescale 1ns / 1ps
import gpu_types_pkg::*;

module gpu_exec_unit #(parameter NUM_LANES = 4) (
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
    output logic [NUM_LANES-1:0]  out_dmem_we,
    output logic [31:0] out_dmem_addr,
    output logic [(NUM_LANES*32)-1:0] out_dmem_wdata,
    input  logic [(NUM_LANES*32)-1:0] in_dmem_rdata,
    
    output logic [NUM_LANES-1:0]  out_flag_zero
);

    logic [31:0] rs1_data [NUM_LANES];
    logic [31:0] rs2_data [NUM_LANES];
    logic [31:0] alu_result [NUM_LANES];
    logic [NUM_LANES-1:0]  flag_zero_bus;
    
    logic [NUM_LANES-1:0] mask_reg;
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) mask_reg <= {NUM_LANES{1'b1}};
        else if (in_alu_op == OP_SETM) mask_reg <= flag_zero_bus;
    end
    assign out_flag_zero = flag_zero_bus;

    assign out_dmem_addr = alu_result[0];
    assign out_dmem_re   = in_mem_re;
    
    always_comb begin
        for (int i=0; i<NUM_LANES; i++) begin
            out_dmem_we[i] = in_mem_we & mask_reg[i];
            out_dmem_wdata[i*32 +: 32] = rs2_data[i]; // STR 存的是 rs2 的数据
        end
    end

    genvar i;
    generate
        for (i=0; i<NUM_LANES; i++) begin : gen_lanes
            // 如果是读内存，写回内存数据；否则，写回ALU计算结果
            logic [31:0] reg_wdata;
            assign reg_wdata = in_mem_re ? in_dmem_rdata[i*32 +: 32] : alu_result[i];
            
            logic reg_we;
            assign reg_we = in_we & mask_reg[i];
            
            gpu_regfile u_reg (
                .clk    (clk),
                .we     (reg_we),
                .waddr  (in_rd_addr),
                .wdata  (reg_wdata),
                .raddr1 (in_rs1_addr),
                .rdata1 (rs1_data[i]),
                .raddr2 (in_rs2_addr),
                .rdata2 (rs2_data[i])
            );
            
            logic [31:0] op_a, op_b;
            assign op_a = rs1_data[i];
            // LDR和STR用立即数做地址偏移，其他算术指令必须用rs2
            assign op_b = (in_alu_op == OP_LDR || in_alu_op == OP_STR) ? in_imm : rs2_data[i];
            
            gpu_alu_lane u_alu (
                .clk       (clk),
                .rst_n     (rst_n),
                .alu_op    (in_alu_op),
                .operand_a (op_a),
                .operand_b (op_b),
                .result    (alu_result[i]),
                .flag_zero (flag_zero_bus[i])
            );
        end
    endgenerate
endmodule