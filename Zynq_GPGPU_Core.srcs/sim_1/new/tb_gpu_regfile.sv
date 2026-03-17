`timescale 1ns / 1ps

module tb_gpu_regfile();

    logic        tb_clk;
    logic        tb_we;
    logic [4:0]  tb_waddr;
    logic [31:0] tb_wdata;
    
    logic [4:0]  tb_raddr1;
    logic [31:0] tb_rdata1;
    
    logic [4:0]  tb_raddr2;
    logic [31:0] tb_rdata2;

    gpu_regfile uut (
        .clk    (tb_clk),
        .we     (tb_we),
        .waddr  (tb_waddr),
        .wdata  (tb_wdata),
        .raddr1 (tb_raddr1),
        .rdata1 (tb_rdata1),
        .raddr2 (tb_raddr2),
        .rdata2 (tb_rdata2)
    );

    // 产生100MHz时钟
    initial begin
        tb_clk = 0;
        forever #5 tb_clk = ~tb_clk;
    end

    // 核心测试流程
    initial begin
        // 初始化状态
        tb_we = 0;
        tb_waddr = 0;
        tb_wdata = 0;
        tb_raddr1 = 0;
        tb_raddr2 = 0;
        #20;

        // 往1号寄存器写入0xDEADBEEF
        @(negedge tb_clk);
        tb_we = 1;
        tb_waddr = 5'd1;
        tb_wdata = 32'hDEADBEEF;
        tb_raddr1 = 5'd1;
        
        // 往5号寄存器写入0x12345678，并同时验证r1的数据
        @(negedge tb_clk);
        tb_we = 1;
        tb_waddr = 5'd5;
        tb_wdata = 32'h12345678;
        tb_raddr1 = 5'd1;
        tb_raddr2 = 5'd5;

        @(negedge tb_clk);
        tb_we = 1;
        tb_waddr = 5'd0;
        tb_wdata = 32'hFFFFFFFF;
        tb_raddr1 = 5'd0;

        @(negedge tb_clk);
        tb_we = 0;
        tb_waddr = 5'd1;
        tb_wdata = 32'h00000000;
        tb_raddr1 = 5'd1;
        #30;
        $display("Register File Simulation Finished!");
        $finish;
    end

endmodule