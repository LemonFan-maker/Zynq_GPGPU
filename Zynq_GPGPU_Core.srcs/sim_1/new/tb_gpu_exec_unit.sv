`timescale 1ns / 1ps
import gpu_types_pkg::*;

module tb_gpu_exec_unit();

    localparam int NUM_LANES = 8;

    logic        tb_clk;
    logic        tb_rst_n;

    logic        tb_in_we;
    logic [4:0]  tb_in_rd_addr;
    logic [4:0]  tb_in_rs1_addr;
    logic [4:0]  tb_in_rs2_addr;
    alu_op_t     tb_in_alu_op;
    logic [31:0] tb_in_imm;
    logic        tb_in_mem_re;
    logic        tb_in_mem_we;
    logic        tb_in_is_addi;
    logic        tb_in_is_branch;
    logic        tb_in_is_mac;
    logic        tb_in_is_mac_acc;
    logic        tb_in_is_mac_acc_nxt;
    logic        tb_in_is_dp4a;
    logic        tb_in_is_mul_ovr;
    logic        tb_in_is_acc_next;
    logic        tb_in_flush;
    logic        tb_in_acc_clr;

    logic        tb_out_dmem_re;
    logic [NUM_LANES-1:0] tb_out_dmem_we;
    logic [31:0] tb_out_dmem_addr;
    logic [NUM_LANES*32-1:0] tb_out_dmem_wdata;
    logic [NUM_LANES*32-1:0] tb_in_dmem_rdata;

    logic [5:0]  tb_in_acc_rd_addr;
    logic [NUM_LANES*32-1:0] tb_out_acc_rd_data;

    logic [NUM_LANES-1:0] tb_out_flag_zero;
    logic        tb_out_branch_taken;
    logic        tb_out_branch_valid;
    logic        tb_out_acc_busy;

    gpu_exec_unit #(.NUM_LANES(NUM_LANES)) uut (
        .clk            (tb_clk),
        .rst_n          (tb_rst_n),
        .in_we          (tb_in_we),
        .in_rd_addr     (tb_in_rd_addr),
        .in_rs1_addr    (tb_in_rs1_addr),
        .in_rs2_addr    (tb_in_rs2_addr),
        .in_alu_op      (tb_in_alu_op),
        .in_imm         (tb_in_imm),
        .in_mem_re      (tb_in_mem_re),
        .in_mem_we      (tb_in_mem_we),
        .in_is_addi     (tb_in_is_addi),
        .in_is_branch   (tb_in_is_branch),
        .in_is_mac      (tb_in_is_mac),
        .in_is_mac_acc  (tb_in_is_mac_acc),
        .in_is_mac_acc_nxt(tb_in_is_mac_acc_nxt),
        .in_is_dp4a     (tb_in_is_dp4a),
        .in_is_mul_ovr  (tb_in_is_mul_ovr),
        .in_is_acc_next (tb_in_is_acc_next),
        .in_flush       (tb_in_flush),
        .in_acc_clr     (tb_in_acc_clr),

        .out_dmem_re    (tb_out_dmem_re),
        .out_dmem_we    (tb_out_dmem_we),
        .out_dmem_addr  (tb_out_dmem_addr),
        .out_dmem_wdata (tb_out_dmem_wdata),
        .in_dmem_rdata  (tb_in_dmem_rdata),

        .in_acc_rd_addr (tb_in_acc_rd_addr),
        .out_acc_rd_data(tb_out_acc_rd_data),

        .out_flag_zero  (tb_out_flag_zero),
        .out_branch_taken(tb_out_branch_taken),
        .out_branch_valid(tb_out_branch_valid),
        .out_acc_busy   (tb_out_acc_busy)
    );

    logic [NUM_LANES*32-1:0] dmem [0:255];

    always_ff @(posedge tb_clk) begin
        if (tb_out_dmem_re)
            tb_in_dmem_rdata <= dmem[tb_out_dmem_addr[7:0]];
    end

    always_ff @(posedge tb_clk) begin
        for (int i = 0; i < NUM_LANES; i++) begin
            if (tb_out_dmem_we[i])
                dmem[tb_out_dmem_addr[7:0]][i*32 +: 32] <= tb_out_dmem_wdata[i*32 +: 32];
        end
    end

    initial begin
        tb_clk = 1'b0;
        forever #5 tb_clk = ~tb_clk;
    end

    initial begin
        tb_rst_n = 1'b0;
        tb_in_we = 1'b0;
        tb_in_rd_addr = 5'd0;
        tb_in_rs1_addr = 5'd0;
        tb_in_rs2_addr = 5'd0;
        tb_in_alu_op = ALU_ADD;
        tb_in_imm = 32'd0;
        tb_in_mem_re = 1'b0;
        tb_in_mem_we = 1'b0;
        tb_in_is_addi = 1'b0;
        tb_in_is_branch = 1'b0;
        tb_in_is_mac = 1'b0;
        tb_in_is_mac_acc = 1'b0;
        tb_in_is_mac_acc_nxt = 1'b0;
        tb_in_is_dp4a = 1'b0;
        tb_in_is_mul_ovr = 1'b0;
        tb_in_is_acc_next = 1'b0;
        tb_in_flush = 1'b0;
        tb_in_acc_clr = 1'b0;
        tb_in_acc_rd_addr = 6'd0;
        tb_in_dmem_rdata = '0;

        dmem[100] = '0;
        dmem[100][31:0] = 32'hDEADBEEF;

        #25;
        tb_rst_n = 1'b1;

        uut.gen_lanes[0].u_rf.regs[1] = 32'd100;

        @(negedge tb_clk);
        tb_in_we       = 1'b1;
        tb_in_mem_re   = 1'b1;
        tb_in_rd_addr  = 5'd3;
        tb_in_rs1_addr = 5'd1;
        tb_in_alu_op   = OP_LDR;

        @(negedge tb_clk);
        tb_in_we       = 1'b0;
        tb_in_mem_re   = 1'b0;
        tb_in_alu_op   = ALU_ADD;

        #40;
        $display("tb_gpu_exec_unit done. branch_valid=%0d acc_busy=%0d", tb_out_branch_valid, tb_out_acc_busy);
        $finish;
    end

endmodule
