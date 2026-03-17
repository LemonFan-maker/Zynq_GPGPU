`timescale 1ns / 1ps
import gpu_types_pkg::*;

module tb_gpu_core_top();
    logic        tb_clk;
    logic        tb_rst_n;
    
    logic [31:0] tb_out_imem_addr;
    logic [31:0] tb_in_imem_data;

    logic        tb_out_dmem_re;   
    logic [3:0]  tb_out_dmem_we;
    logic [31:0] tb_out_dmem_addr; 
    logic [127:0] tb_out_dmem_wdata; 
    logic [127:0] tb_in_dmem_rdata;  

    logic [3:0]  tb_out_flag_zero;

    gpu_core_top uut (
        .clk             (tb_clk),
        .rst_n           (tb_rst_n),
        .out_imem_addr   (tb_out_imem_addr),
        .in_imem_data    (tb_in_imem_data),
        .out_dmem_re     (tb_out_dmem_re),
        .out_dmem_we     (tb_out_dmem_we),
        .out_dmem_addr   (tb_out_dmem_addr),
        .out_dmem_wdata  (tb_out_dmem_wdata),
        .in_dmem_rdata   (tb_in_dmem_rdata),
        .out_flag_zero   (tb_out_flag_zero)
    );

    // 内存初始化清零
    logic [31:0] imem [0:15] = '{default: '0};
    assign tb_in_imem_data = imem[tb_out_imem_addr[3:0]];

    logic [127:0] dmem [0:255] = '{default: '0};
    
    // 内存读取
    always_ff @(posedge tb_clk) begin
        if (tb_out_dmem_re) tb_in_dmem_rdata <= dmem[tb_out_dmem_addr[7:0]];
    end
    
    always_ff @(posedge tb_clk) begin
        for (int i=0; i<4; i++) begin
            if (tb_out_dmem_we[i]) begin
                dmem[tb_out_dmem_addr[7:0]][i*32 +: 32] <= tb_out_dmem_wdata[i*32 +: 32];
            end
        end
    end

    // 时钟
    initial begin
        tb_clk = 0; forever #5 tb_clk = ~tb_clk;
    end

    // 测试流程
    initial begin
        dmem[10] = {32'd100, 32'd30, 32'd100, 32'd10}; 
        
        dmem[14] = {32'd100, 32'd100, 32'd100, 32'd100}; 

        imem[0] = 32'h8080000A; // LDR r1, [r0+10]
        imem[1] = 32'h8100000E; // LDR r2, [r0+14]
        imem[2] = 32'h00000000; imem[3] = 32'h00000000;   
           
        imem[4] = 32'hA0004100;
        
        imem[5] = 32'h01802100;
        imem[6] = 32'h00000000; imem[7] = 32'h00000000;
        
        imem[8] = 32'h90006014;
        
        tb_rst_n = 0; #25; tb_rst_n = 1;
        #200; $finish;
    end
endmodule