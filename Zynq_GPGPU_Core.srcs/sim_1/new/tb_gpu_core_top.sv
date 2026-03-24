`timescale 1ns / 1ps
import gpu_types_pkg::*;

module tb_gpu_core_top();
    localparam int TB_TIMEOUT_CYCLES = 5000;

    logic        tb_clk;
    logic        tb_rst_n;
    logic        tb_start_pulse;
    
    logic [31:0] tb_out_imem_addr;
    logic [31:0] tb_in_imem_data;

    logic        tb_out_dmem_re;
    logic [7:0]  tb_out_dmem_we;
    logic [31:0] tb_out_dmem_addr;
    logic [255:0] tb_out_dmem_wdata;
    logic [255:0] tb_in_dmem_rdata;

    logic [7:0]  tb_out_flag_zero;

    logic        tb_in_acc_clr;
    logic [5:0]  tb_in_acc_rd_addr;
    logic [255:0] tb_out_acc_rd_data;
    logic        tb_gpu_done;

    function automatic [31:0] enc_i13(input [3:0] op, input [4:0] rd, input [4:0] rs1, input integer imm13);
        logic [12:0] s;
        begin
            s = imm13[12:0];
            enc_i13 = {op, rd, 5'd0, s[12:8], rs1, s[7:0]};
        end
    endfunction

    function automatic [31:0] enc_beq(input [4:0] rs1, input [4:0] rs2, input integer imm13);
        logic [12:0] s;
        begin
            s = imm13[12:0];
            enc_beq = {4'hD, rs2, 5'd0, s[12:8], rs1, s[7:0]};
        end
    endfunction

    function automatic [31:0] enc_bne(input [4:0] rs1, input [4:0] rs2, input integer imm13);
        logic [12:0] s;
        begin
            s = imm13[12:0];
            enc_bne = {4'hE, rs2, 5'd0, s[12:8], rs1, s[7:0]};
        end
    endfunction

    function automatic [31:0] enc_i8(input [3:0] op, input [4:0] rd, input [4:0] rs1, input integer imm8);
        logic [7:0] s;
        begin
            s = imm8[7:0];
            enc_i8 = {op, rd, 5'd0, 5'd0, rs1, s};
        end
    endfunction

    gpu_core_top uut (
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

    logic [31:0] imem [0:63] = '{default: '0};
    assign tb_in_imem_data = imem[tb_out_imem_addr[5:0]];

    logic [255:0] dmem [0:255] = '{default: '0};
    
    // 内存读取
    always_ff @(posedge tb_clk) begin
        if (tb_out_dmem_re) tb_in_dmem_rdata <= dmem[tb_out_dmem_addr[7:0]];
    end
    
    always_ff @(posedge tb_clk) begin
        for (int i=0; i<8; i++) begin
            if (tb_out_dmem_we[i]) begin
                dmem[tb_out_dmem_addr[7:0]][i*32 +: 32] <= tb_out_dmem_wdata[i*32 +: 32];
            end
        end
    end

    initial begin
        tb_clk = 0; forever #5 tb_clk = ~tb_clk;
    end

    initial begin
        tb_rst_n = 1'b0;
        tb_start_pulse = 1'b0;
        tb_in_acc_clr = 1'b0;
        tb_in_acc_rd_addr = 6'd0;
        dmem[12][31:0] = 32'd42;
        dmem[20][31:0] = 32'd100;

        imem[0] = enc_i13(4'hC, 5'd1, 5'd0, 13'd1);
        imem[1] = enc_i13(4'hC, 5'd2, 5'd0, 13'd1);
        imem[2] = enc_beq(5'd1, 5'd2, 13'd2);
        imem[3] = enc_i13(4'hC, 5'd3, 5'd0, 13'd7);
        imem[4] = enc_i13(4'hC, 5'd3, 5'd0, 13'd9);
        imem[5] = enc_i13(4'hC, 5'd4, 5'd0, 13'd5);
        imem[6] = enc_i13(4'hC, 5'd5, 5'd4, 13'd3);
        imem[7] = enc_i13(4'hC, 5'd6, 5'd5, 13'd4);
        imem[8]  = enc_i13(4'hC, 5'd10, 5'd0, 13'd12);
        imem[9]  = enc_i8 (4'h8, 5'd7, 5'd10, 8'd0);
        imem[10] = enc_i13(4'hC, 5'd8, 5'd7, 13'd1);
        imem[11] = enc_i13(4'hC, 5'd9, 5'd0, 13'd0);
        imem[12] = enc_beq(5'd9, 5'd9, 13'd2);
        imem[13] = enc_i13(4'hC, 5'd11, 5'd0, 13'd111);
        imem[14] = enc_beq(5'd9, 5'd9, 13'd2);
        imem[15] = enc_i13(4'hC, 5'd11, 5'd0, 13'd222);
        imem[16] = enc_i13(4'hC, 5'd11, 5'd0, 13'd333);
        imem[17] = enc_i13(4'hC, 5'd12, 5'd0, 13'd20);
        imem[18] = enc_i8 (4'h8, 5'd13, 5'd12, 8'd0);
        imem[19] = enc_i13(4'hC, 5'd14, 5'd13, 13'd2);
        imem[20] = enc_i8 (4'h8, 5'd15, 5'd12, 8'd0);
        imem[21] = enc_i13(4'hC, 5'd16, 5'd15, 13'd3);
        imem[22] = enc_i13(4'hC, 5'd17, 5'd0, 13'd12);
        imem[23] = enc_beq(5'd9, 5'd9, 13'd2);
        imem[24] = enc_i8 (4'h8, 5'd18, 5'd17, 8'd0);
        imem[25] = enc_i8 (4'h8, 5'd18, 5'd17, 8'd0);
        imem[26] = enc_i13(4'hC, 5'd19, 5'd18, 13'd5);
        imem[27] = enc_beq(5'd9, 5'd9, 13'd2);
        imem[28] = enc_i13(4'hC, 5'd20, 5'd0, 13'd77);
        imem[29] = enc_i13(4'hC, 5'd20, 5'd0, 13'd88);
        imem[30] = enc_i13(4'hC, 5'd21, 5'd0, 13'd1);
        imem[31] = enc_i13(4'hC, 5'd22, 5'd0, 13'd2);
        imem[32] = enc_beq(5'd21, 5'd22, 13'd2);
        imem[33] = enc_i13(4'hC, 5'd23, 5'd0, 13'd55);
        imem[34] = enc_bne(5'd21, 5'd22, 13'd2);
        imem[35] = enc_i13(4'hC, 5'd23, 5'd0, 13'd66);
        imem[36] = enc_i13(4'hC, 5'd23, 5'd0, 13'd77);
        imem[37] = 32'h00000001;

        #25;
        tb_rst_n = 1'b1;

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

        #20;

        if (uut.u_exec_unit.gen_lanes[0].u_rf.regs[3] !== 32'd9) begin
            $error("Branch flush FAILED: r3=%0d expected 9", uut.u_exec_unit.gen_lanes[0].u_rf.regs[3]);
        end else begin
            $display("Branch flush PASS: r3=%0d", uut.u_exec_unit.gen_lanes[0].u_rf.regs[3]);
        end

        if (uut.u_exec_unit.gen_lanes[0].u_rf.regs[6] !== 32'd12) begin
            $error("Forwarding FAILED: r6=%0d expected 12", uut.u_exec_unit.gen_lanes[0].u_rf.regs[6]);
        end else begin
            $display("Forwarding PASS: r6=%0d", uut.u_exec_unit.gen_lanes[0].u_rf.regs[6]);
        end

        if (uut.u_exec_unit.gen_lanes[0].u_rf.regs[8] !== 32'd43) begin
            $error("Load-use FAILED: r8=%0d expected 43", uut.u_exec_unit.gen_lanes[0].u_rf.regs[8]);
        end else begin
            $display("Load-use PASS: r8=%0d", uut.u_exec_unit.gen_lanes[0].u_rf.regs[8]);
        end

        if (uut.u_exec_unit.gen_lanes[0].u_rf.regs[11] !== 32'd333) begin
            $error("Back-to-back branch FAILED: r11=%0d expected 333", uut.u_exec_unit.gen_lanes[0].u_rf.regs[11]);
        end else begin
            $display("Back-to-back branch PASS: r11=%0d", uut.u_exec_unit.gen_lanes[0].u_rf.regs[11]);
        end

        if (uut.u_exec_unit.gen_lanes[0].u_rf.regs[14] !== 32'd102) begin
            $error("Load-use #2 FAILED: r14=%0d expected 102", uut.u_exec_unit.gen_lanes[0].u_rf.regs[14]);
        end else begin
            $display("Load-use #2 PASS: r14=%0d", uut.u_exec_unit.gen_lanes[0].u_rf.regs[14]);
        end

        if (uut.u_exec_unit.gen_lanes[0].u_rf.regs[16] !== 32'd103) begin
            $error("Load-use #3 FAILED: r16=%0d expected 103", uut.u_exec_unit.gen_lanes[0].u_rf.regs[16]);
        end else begin
            $display("Load-use #3 PASS: r16=%0d", uut.u_exec_unit.gen_lanes[0].u_rf.regs[16]);
        end

        if (uut.u_exec_unit.gen_lanes[0].u_rf.regs[19] !== 32'd47) begin
            $error("Branch-load interleave FAILED: r19=%0d expected 47", uut.u_exec_unit.gen_lanes[0].u_rf.regs[19]);
        end else begin
            $display("Branch-load interleave PASS: r19=%0d", uut.u_exec_unit.gen_lanes[0].u_rf.regs[19]);
        end

        if (uut.u_exec_unit.gen_lanes[0].u_rf.regs[20] !== 32'd88) begin
            $error("Interleave branch flush FAILED: r20=%0d expected 88", uut.u_exec_unit.gen_lanes[0].u_rf.regs[20]);
        end else begin
            $display("Interleave branch flush PASS: r20=%0d", uut.u_exec_unit.gen_lanes[0].u_rf.regs[20]);
        end

        if (uut.u_exec_unit.gen_lanes[0].u_rf.regs[23] !== 32'd77) begin
            $error("Branch mix FAILED: r23=%0d expected 77", uut.u_exec_unit.gen_lanes[0].u_rf.regs[23]);
        end else begin
            $display("Branch mix PASS: r23=%0d", uut.u_exec_unit.gen_lanes[0].u_rf.regs[23]);
        end

        $display("DBG counters: cycles=%0d flush=%0d stall_total=%0d stall_acc=%0d stall_load=%0d",
                 uut.dbg_cycle_count,
                 uut.dbg_flush_count,
                 uut.dbg_stall_total_count,
                 uut.dbg_stall_acc_count,
                 uut.dbg_stall_load_count);

        $display("DBG pipeline: issue=%0d redirect=%0d",
             uut.dbg_issue_count,
             uut.dbg_redirect_count);

        if (uut.dbg_flush_count < 32'd6) begin
            $error("Flush counter FAILED: flush=%0d expected >= 6", uut.dbg_flush_count);
        end

        if (uut.dbg_stall_load_count < 32'd4) begin
            $error("Load-stall counter FAILED: stall_load=%0d expected >= 4", uut.dbg_stall_load_count);
        end

        if (uut.dbg_stall_acc_count !== 32'd0) begin
            $error("ACC-stall counter FAILED: stall_acc=%0d expected 0 for this program", uut.dbg_stall_acc_count);
        end

        $finish;
    end
endmodule