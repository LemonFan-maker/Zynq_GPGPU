`timescale 1ns / 1ps
import gpu_types_pkg::*;

module gpu_core_top (
    input  logic        clk,
    input  logic        rst_n,
    input  logic        start_pulse,

    output logic [31:0] out_imem_addr,
    input  logic [31:0] in_imem_data,

    output logic        out_dmem_re,
    output logic [7:0]  out_dmem_we,
    output logic [31:0] out_dmem_addr,
    output logic [255:0] out_dmem_wdata,
    input  logic [255:0] in_dmem_rdata,

    output logic [7:0]  out_flag_zero,

    // Accumulator buffer interface
    input  logic        in_acc_clr,
    input  logic [5:0]  in_acc_rd_addr,
    output logic [255:0] out_acc_rd_data,

    output logic        gpu_done
);

    // PC 寄存器
    logic [31:0] pc;
    assign out_imem_addr = pc;

    // Decoder 输出
    logic        dec_we;
    logic [4:0]  dec_rd_addr;
    logic [4:0]  dec_rs1_addr;
    logic [4:0]  dec_rs2_addr;
    alu_op_t     dec_alu_op;
    logic [31:0] dec_imm;
    logic        dec_mem_re;
    logic        dec_mem_we;
    logic        dec_is_branch;
    logic        dec_is_jump;
    logic        dec_is_addi;
    logic        dec_is_mac;
    logic        dec_is_mac_acc;
    logic        dec_is_mac_acc_nxt;
    logic        dec_is_mul_ovr;
    logic        dec_is_acc_next;
    logic        dec_halt;

    // 分支判断
    logic        branch_taken;

    // HALT 状态
    logic        halted;
    assign gpu_done = halted;

    // 取指+解码都是组合逻辑，不存在流水线气泡问题
    // PC 跳转后，下一周期自然解码目标地址的指令，无需 flush

    // PC 逻辑
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            pc     <= 32'h0;
            halted <= 1'b0;
        end else if (start_pulse) begin
            pc     <= 32'h0;
            halted <= 1'b0;
        end else if (!halted) begin
            if (dec_halt) begin
                halted <= 1'b1;
            end else if (dec_is_jump) begin
                pc <= {19'b0, dec_imm[12:0]};
            end else if (dec_is_branch && branch_taken) begin
                pc <= pc + dec_imm;
            end else begin
                pc <= pc + 1'b1;
            end
        end
    end

    gpu_decoder u_decoder (
        .instruction  (in_imem_data),
        .out_we       (dec_we),
        .out_rd_addr  (dec_rd_addr),
        .out_rs1_addr (dec_rs1_addr),
        .out_rs2_addr (dec_rs2_addr),
        .out_alu_op   (dec_alu_op),
        .out_imm      (dec_imm),
        .out_mem_re   (dec_mem_re),
        .out_mem_we   (dec_mem_we),
        .out_is_branch(dec_is_branch),
        .out_is_jump  (dec_is_jump),
        .out_is_addi  (dec_is_addi),
        .out_is_mac   (dec_is_mac),
        .out_is_mac_acc(dec_is_mac_acc),
        .out_is_mac_acc_nxt(dec_is_mac_acc_nxt),
        .out_is_mul_ovr(dec_is_mul_ovr),
        .out_is_acc_next(dec_is_acc_next),
        .out_halt     (dec_halt)
    );

    gpu_exec_unit #( .NUM_LANES(8) ) u_exec_unit (
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
        .in_is_addi     (dec_is_addi),
        .in_is_mac      (dec_is_mac),
        .in_is_mac_acc  (dec_is_mac_acc),
        .in_is_mac_acc_nxt(dec_is_mac_acc_nxt),
        .in_is_mul_ovr  (dec_is_mul_ovr),
        .in_is_acc_next (dec_is_acc_next),
        .in_flush       (1'b0),
        .in_acc_clr     (in_acc_clr),

        .out_dmem_re    (out_dmem_re),
        .out_dmem_we    (out_dmem_we),
        .out_dmem_addr  (out_dmem_addr),
        .out_dmem_wdata (out_dmem_wdata),
        .in_dmem_rdata  (in_dmem_rdata),

        .in_acc_rd_addr (in_acc_rd_addr),
        .out_acc_rd_data(out_acc_rd_data),

        .out_flag_zero  (out_flag_zero),
        .out_branch_taken(branch_taken)
    );

endmodule
