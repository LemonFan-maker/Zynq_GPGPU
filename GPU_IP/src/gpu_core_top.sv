`timescale 1ns / 1ps
import gpu_types_pkg::*;

module gpu_core_top #(
    parameter int NUM_LANES = 16
) (
    input  logic        clk,
    input  logic        rst_n,
    input  logic        start_pulse,

    output logic [31:0] out_imem_addr,
    input  logic [31:0] in_imem_data,

    output logic        out_dmem_re,
    output logic [NUM_LANES-1:0]  out_dmem_we,
    output logic [31:0] out_dmem_addr,
    output logic [NUM_LANES*32-1:0] out_dmem_wdata,
    input  logic [NUM_LANES*32-1:0] in_dmem_rdata,

    output logic [NUM_LANES-1:0]  out_flag_zero,

    input  logic        in_acc_clr,
    input  logic [5:0]  in_acc_rd_addr,
    output logic [NUM_LANES*32-1:0] out_acc_rd_data,

    output logic        gpu_done
);

    logic [31:0] pc;
    assign out_imem_addr = pc;

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
    logic        dec_is_dp4a;
    logic        dec_is_mul_ovr;
    logic        dec_is_acc_next;
    logic        dec_halt;

    logic        branch_taken;
    logic        halted;
    assign gpu_done = halted;

    logic [31:0] id_ex_pc;
    logic        id_ex_valid;
    logic        id_ex_we;
    (* max_fanout = 8 *) logic [4:0] id_ex_rd_addr;
    (* max_fanout = 8 *) logic [4:0] id_ex_rs1_addr;
    (* max_fanout = 8 *) logic [4:0] id_ex_rs2_addr;
    alu_op_t     id_ex_alu_op;
    logic [31:0] id_ex_imm;
    logic        id_ex_mem_re;
    logic        id_ex_mem_we;
    logic        id_ex_is_branch;
    logic        id_ex_is_jump;
    logic        id_ex_is_addi;
    logic        id_ex_is_mac;
    logic        id_ex_is_mac_acc;
    logic        id_ex_is_mac_acc_nxt;
    logic        id_ex_is_dp4a;
    logic        id_ex_is_mul_ovr;
    logic        id_ex_is_acc_next;
    logic        id_ex_halt;

    logic ctrl_redirect;
    assign ctrl_redirect = id_ex_halt || id_ex_is_jump || (id_ex_is_branch && branch_taken);

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            pc     <= 32'h0;
            halted <= 1'b1;
        end else if (start_pulse) begin
            pc     <= 32'h0;
            halted <= 1'b0;
        end else if (!halted) begin
            if (id_ex_halt) begin
                halted <= 1'b1;
            end else if (id_ex_is_jump) begin
                pc <= {19'b0, id_ex_imm[12:0]};
            end else if (id_ex_is_branch && branch_taken) begin
                pc <= id_ex_pc + id_ex_imm;
            end else begin
                pc <= pc + 1'b1;
            end
        end
    end

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            id_ex_pc            <= 32'h0;
            id_ex_valid         <= 1'b0;
            id_ex_we            <= 1'b0;
            id_ex_rd_addr       <= 5'd0;
            id_ex_rs1_addr      <= 5'd0;
            id_ex_rs2_addr      <= 5'd0;
            id_ex_alu_op        <= ALU_ADD;
            id_ex_imm           <= 32'h0;
            id_ex_mem_re        <= 1'b0;
            id_ex_mem_we        <= 1'b0;
            id_ex_is_branch     <= 1'b0;
            id_ex_is_jump       <= 1'b0;
            id_ex_is_addi       <= 1'b0;
            id_ex_is_mac        <= 1'b0;
            id_ex_is_mac_acc    <= 1'b0;
            id_ex_is_mac_acc_nxt<= 1'b0;
            id_ex_is_dp4a       <= 1'b0;
            id_ex_is_mul_ovr    <= 1'b0;
            id_ex_is_acc_next   <= 1'b0;
            id_ex_halt          <= 1'b0;
        end else if (start_pulse) begin
            id_ex_valid         <= 1'b0;
            id_ex_we            <= 1'b0;
            id_ex_mem_re        <= 1'b0;
            id_ex_mem_we        <= 1'b0;
            id_ex_is_branch     <= 1'b0;
            id_ex_is_jump       <= 1'b0;
            id_ex_is_addi       <= 1'b0;
            id_ex_is_mac        <= 1'b0;
            id_ex_is_mac_acc    <= 1'b0;
            id_ex_is_mac_acc_nxt<= 1'b0;
            id_ex_is_dp4a       <= 1'b0;
            id_ex_is_mul_ovr    <= 1'b0;
            id_ex_is_acc_next   <= 1'b0;
            id_ex_halt          <= 1'b0;
        end else if (!halted) begin
            if (ctrl_redirect) begin
                id_ex_valid         <= 1'b0;
                id_ex_we            <= 1'b0;
                id_ex_mem_re        <= 1'b0;
                id_ex_mem_we        <= 1'b0;
                id_ex_is_branch     <= 1'b0;
                id_ex_is_jump       <= 1'b0;
                id_ex_is_addi       <= 1'b0;
                id_ex_is_mac        <= 1'b0;
                id_ex_is_mac_acc    <= 1'b0;
                id_ex_is_mac_acc_nxt<= 1'b0;
                id_ex_is_dp4a       <= 1'b0;
                id_ex_is_mul_ovr    <= 1'b0;
                id_ex_is_acc_next   <= 1'b0;
                id_ex_halt          <= 1'b0;
            end else begin
                id_ex_pc            <= pc;
                id_ex_valid         <= 1'b1;
                id_ex_we            <= dec_we;
                id_ex_rd_addr       <= dec_rd_addr;
                id_ex_rs1_addr      <= dec_rs1_addr;
                id_ex_rs2_addr      <= dec_rs2_addr;
                id_ex_alu_op        <= dec_alu_op;
                id_ex_imm           <= dec_imm;
                id_ex_mem_re        <= dec_mem_re;
                id_ex_mem_we        <= dec_mem_we;
                id_ex_is_branch     <= dec_is_branch;
                id_ex_is_jump       <= dec_is_jump;
                id_ex_is_addi       <= dec_is_addi;
                id_ex_is_mac        <= dec_is_mac;
                id_ex_is_mac_acc    <= dec_is_mac_acc;
                id_ex_is_mac_acc_nxt<= dec_is_mac_acc_nxt;
                id_ex_is_dp4a       <= dec_is_dp4a;
                id_ex_is_mul_ovr    <= dec_is_mul_ovr;
                id_ex_is_acc_next   <= dec_is_acc_next;
                id_ex_halt          <= dec_halt;
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
        .out_is_dp4a  (dec_is_dp4a),
        .out_is_mul_ovr(dec_is_mul_ovr),
        .out_is_acc_next(dec_is_acc_next),
        .out_halt     (dec_halt)
    );

    gpu_exec_unit #(.NUM_LANES(NUM_LANES)) u_exec_unit (
        .clk            (clk),
        .rst_n          (rst_n),
        .in_we          (id_ex_we),
        .in_rd_addr     (id_ex_rd_addr),
        .in_rs1_addr    (id_ex_rs1_addr),
        .in_rs2_addr    (id_ex_rs2_addr),
        .in_alu_op      (id_ex_alu_op),
        .in_imm         (id_ex_imm),
        .in_mem_re      (id_ex_mem_re),
        .in_mem_we      (id_ex_mem_we),
        .in_is_addi     (id_ex_is_addi),
        .in_is_mac      (id_ex_is_mac),
        .in_is_mac_acc  (id_ex_is_mac_acc),
        .in_is_mac_acc_nxt(id_ex_is_mac_acc_nxt),
        .in_is_dp4a     (id_ex_is_dp4a),
        .in_is_mul_ovr  (id_ex_is_mul_ovr),
        .in_is_acc_next (id_ex_is_acc_next),
        .in_flush       (start_pulse),
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
