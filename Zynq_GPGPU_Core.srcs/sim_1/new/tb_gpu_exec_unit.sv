`timescale 1ns / 1ps
import gpu_types_pkg::*;

module tb_gpu_exec_unit();

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
    
    logic        tb_out_dmem_re;
    logic        tb_out_dmem_we;
    logic [31:0] tb_out_dmem_addr;
    logic [31:0] tb_out_dmem_wdata;
    logic [31:0] tb_in_dmem_rdata;

    logic        tb_out_flag_zero;

    gpu_exec_unit uut (
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
        
        .out_dmem_re    (tb_out_dmem_re),
        .out_dmem_we    (tb_out_dmem_we),
        .out_dmem_addr  (tb_out_dmem_addr),
        .out_dmem_wdata (tb_out_dmem_wdata),
        .in_dmem_rdata  (tb_in_dmem_rdata),
        
        .out_flag_zero  (tb_out_flag_zero)
    );
    
    logic [31:0] dmem [0:255];

    // DMEM读逻辑
    always_ff @(posedge tb_clk) begin
        if (tb_out_dmem_re) begin
            tb_in_dmem_rdata <= dmem[tb_out_dmem_addr[7:0]]; 
        end
    end

    // DMEM写逻辑
    always_ff @(posedge tb_clk) begin
        if (tb_out_dmem_we) begin
            dmem[tb_out_dmem_addr[7:0]] <= tb_out_dmem_wdata;
        end
    end

    initial begin
        tb_clk = 0;
        forever #5 tb_clk = ~tb_clk;
    end

    initial begin
        tb_rst_n = 0;
        tb_in_we = 0; tb_in_mem_re = 0; tb_in_mem_we = 0;
        
        dmem[100] = 32'hDEADBEEF; 
        
        #25;
        tb_rst_n = 1;
        
        uut.u_rf.regs[1] = 32'd100;
        uut.u_rf.regs[2] = 32'h88889999;

        @(negedge tb_clk);
        tb_in_we       = 1'b1;
        tb_in_mem_re   = 1'b1;    // 开启内存读
        tb_in_mem_we   = 1'b0;
        tb_in_rd_addr  = 5'd3;
        tb_in_rs1_addr = 5'd1;
        tb_in_rs2_addr = 5'd0;
        tb_in_alu_op   = OP_LDR;
        tb_in_imm      = 32'd0;   // 偏移量为 0

        @(negedge tb_clk);
        tb_in_we       = 1'b0;
        tb_in_mem_re   = 1'b0;
        tb_in_mem_we   = 1'b1;    // 开启内存写
        tb_in_rd_addr  = 5'd0;
        tb_in_rs1_addr = 5'd1;
        tb_in_rs2_addr = 5'd2;
        tb_in_alu_op   = OP_STR;
        tb_in_imm      = 32'hFFFFFFCE; 

        @(negedge tb_clk);
        tb_in_mem_we   = 1'b0;
        tb_in_alu_op   = ALU_ADD;

        #50;
        $display("LSU Execution Unit Simulation Finished!");
        $finish;
    end

endmodule