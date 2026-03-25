`timescale 1ns / 1ps
import gpu_types_pkg::*;

module tb_gpu_core_top_dp4a();
    localparam int TB_TIMEOUT_CYCLES = 2000;
    localparam int NUM_LANES = 16;
    localparam int DMEM_DATA_W = NUM_LANES * 32;

    logic        tb_clk;
    logic        tb_rst_n;
    logic        tb_start_pulse;

    logic [31:0] tb_out_imem_addr;
    logic [31:0] tb_in_imem_data;

    logic        tb_out_dmem_re;
    logic [NUM_LANES-1:0]  tb_out_dmem_we;
    logic [31:0] tb_out_dmem_addr;
    logic [DMEM_DATA_W-1:0] tb_out_dmem_wdata;
    logic [DMEM_DATA_W-1:0] tb_in_dmem_rdata;

    logic [NUM_LANES-1:0]  tb_out_flag_zero;

    logic        tb_in_acc_clr;
    logic [5:0]  tb_in_acc_rd_addr;
    logic [DMEM_DATA_W-1:0] tb_out_acc_rd_data;
    logic        tb_gpu_done;

    function automatic [31:0] enc_i13(input [3:0] op, input [4:0] rd, input [4:0] rs1, input integer imm13);
        logic [12:0] s;
        begin
            s = imm13[12:0];
            enc_i13 = {op, rd, 5'd0, s[12:8], rs1, s[7:0]};
        end
    endfunction

    function automatic [31:0] enc_i8(input [3:0] op, input [4:0] rd, input [4:0] rs1, input integer imm8);
        logic [7:0] s;
        begin
            s = imm8[7:0];
            enc_i8 = {op, rd, 5'd0, 5'd0, rs1, s};
        end
    endfunction

    function automatic [31:0] enc_r(input [3:0] op, input [4:0] rd, input [4:0] rs1, input [4:0] rs2);
        begin
            enc_r = {op, rd, 5'd0, rs2, rs1, 8'd0};
        end
    endfunction

    gpu_core_top #(
        .NUM_LANES(NUM_LANES)
    ) uut (
        .clk             (tb_clk),
        .rst_n           (tb_rst_n),
        .start_pulse     (tb_start_pulse),
        .out_imem_addr   (tb_out_imem_addr),
        .in_imem_data    (tb_in_imem_data),
        .out_dmem_re     (tb_out_dmem_re),
        .out_dmem_we     (tb_out_dmem_we),
        .out_dmem_addr   (tb_out_dmem_addr),
        .out_dmem_wdata  (tb_out_dmem_wdata),
        .in_dmem_rdata   (tb_in_dmem_rdata),
        .out_flag_zero   (tb_out_flag_zero),
        .in_acc_clr      (tb_in_acc_clr),
        .in_acc_rd_addr  (tb_in_acc_rd_addr),
        .out_acc_rd_data (tb_out_acc_rd_data),
        .gpu_done        (tb_gpu_done)
    );

    logic [31:0] imem [0:63];
    logic [DMEM_DATA_W-1:0] dmem [0:255];

    assign tb_in_imem_data = imem[tb_out_imem_addr[5:0]];

    always_ff @(posedge tb_clk) begin
        if (tb_out_dmem_re) begin
            tb_in_dmem_rdata <= dmem[tb_out_dmem_addr[7:0]];
        end
    end

    always_ff @(posedge tb_clk) begin
        for (int i = 0; i < NUM_LANES; i++) begin
            if (tb_out_dmem_we[i]) begin
                dmem[tb_out_dmem_addr[7:0]][i*32 +: 32] <= tb_out_dmem_wdata[i*32 +: 32];
            end
        end
    end

    initial begin
        for (int i = 0; i < 64; i++) begin
            imem[i] = 32'h0;
        end
        for (int i = 0; i < 256; i++) begin
            dmem[i] = '0;
        end

        tb_clk = 1'b0;
        forever #5 tb_clk = ~tb_clk;
    end

    localparam int EXP_DP4A = (1*5) + (2*6) + (3*7) + (4*8); // 70

    initial begin
        tb_rst_n = 1'b0;
        tb_start_pulse = 1'b0;
        tb_in_acc_clr = 1'b0;
        tb_in_acc_rd_addr = 6'd0;

        dmem[30][31:0] = 32'h04030201;
        dmem[31][31:0] = 32'h08070605;

        imem[0] = enc_i13(4'hC, 5'd10, 5'd0, 13'd30);
        imem[1] = enc_i8 (4'h8, 5'd1,  5'd10, 8'd0);
        imem[2] = enc_i8 (4'h8, 5'd2,  5'd10, 8'd1);
        imem[3] = enc_r  (4'h2, 5'd3,  5'd1,  5'd2) | 32'h00000080; // DP4A
        imem[4] = enc_r  (4'h2, 5'd0,  5'd1,  5'd2) | 32'h00000082; // DP4A_ACC
        imem[5] = enc_r  (4'h2, 5'd0,  5'd0,  5'd0) | 32'h00000003; // ACC_NEXT
        imem[6] = 32'h00000001; // HALT

        #25;
        tb_rst_n = 1'b1;

        @(negedge tb_clk);
        tb_in_acc_clr = 1'b1;
        @(negedge tb_clk);
        tb_in_acc_clr = 1'b0;
        repeat (80) @(posedge tb_clk);

        @(negedge tb_clk);
        tb_start_pulse = 1'b1;
        @(negedge tb_clk);
        tb_start_pulse = 1'b0;

        fork
            begin
                wait(tb_gpu_done == 1'b1);
            end
            begin
                repeat (TB_TIMEOUT_CYCLES) @(posedge tb_clk);
                $fatal(1, "TB timeout: gpu_done not asserted within %0d cycles", TB_TIMEOUT_CYCLES);
            end
        join_any
        disable fork;

        tb_in_acc_rd_addr = 6'd0;
        #10;

        $display("DP4A OBS: r1=0x%08h r2=0x%08h r3=%0d acc0=%0h",
                 uut.u_exec_unit.gen_lanes[0].u_rf.regs[1],
                 uut.u_exec_unit.gen_lanes[0].u_rf.regs[2],
                 uut.u_exec_unit.gen_lanes[0].u_rf.regs[3],
                 tb_out_acc_rd_data[31:0]);

        if (uut.u_exec_unit.gen_lanes[0].u_rf.regs[1] !== 32'h04030201)
            $error("LDR-A FAIL: r1=0x%08h expected 0x04030201", uut.u_exec_unit.gen_lanes[0].u_rf.regs[1]);
        if (uut.u_exec_unit.gen_lanes[0].u_rf.regs[2] !== 32'h08070605)
            $error("LDR-B FAIL: r2=0x%08h expected 0x08070605", uut.u_exec_unit.gen_lanes[0].u_rf.regs[2]);

        if (uut.u_exec_unit.gen_lanes[0].u_rf.regs[3] !== EXP_DP4A[31:0]) begin
            $error("DP4A FAIL: r3=%0d expected %0d", uut.u_exec_unit.gen_lanes[0].u_rf.regs[3], EXP_DP4A);
        end else begin
            $display("DP4A PASS: r3=%0d", uut.u_exec_unit.gen_lanes[0].u_rf.regs[3]);
        end

        if (tb_out_acc_rd_data[31:0] !== EXP_DP4A[31:0]) begin
            $error("DP4A_ACC FAIL: acc[0]=%0d expected %0d", tb_out_acc_rd_data[31:0], EXP_DP4A);
        end else begin
            $display("DP4A_ACC PASS: acc[0]=%0d", tb_out_acc_rd_data[31:0]);
        end

        $display("DP4A DBG: cycles=%0d issue=%0d stall_load=%0d stall_acc=%0d",
                 uut.dbg_cycle_count,
                 uut.dbg_issue_count,
                 uut.dbg_stall_load_count,
                 uut.dbg_stall_acc_count);

        if (uut.dbg_cycle_count > 32'd120) begin
            $error("DP4A PERF REGRESSION: cycles=%0d exceeds budget 120", uut.dbg_cycle_count);
        end

        $finish;
    end
endmodule
