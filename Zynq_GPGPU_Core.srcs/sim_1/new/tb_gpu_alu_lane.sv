`timescale 1ns / 1ps
import gpu_types_pkg::*; 

module tb_gpu_alu_lane();

    // 时钟和复位信号
    logic         tb_clk;
    logic         tb_rst_n;
    alu_op_t      tb_alu_op;
    logic [31:0]  tb_operand_a;
    logic [31:0]  tb_operand_b;
    logic [31:0]  tb_result;
    logic         tb_flag_zero;

    gpu_alu_lane uut (
        .clk       (tb_clk),
        .rst_n     (tb_rst_n),
        .alu_op    (tb_alu_op),
        .operand_a (tb_operand_a),
        .operand_b (tb_operand_b),
        .result    (tb_result),
        .flag_zero (tb_flag_zero)
    );

    // 产生100MHz时钟
    initial begin
        tb_clk = 0;
        forever #5 tb_clk = ~tb_clk;
    end

    initial begin
        // 系统复位
        tb_rst_n = 0;
        tb_alu_op = ALU_ADD;
        tb_operand_a = 32'd0;
        tb_operand_b = 32'd0;
        
        #20 tb_rst_n = 1; 
        
        // 在时钟下降沿给数据，模拟上游模块的输出
        @(negedge tb_clk); 
        tb_alu_op = ALU_ADD;
        tb_operand_a = 32'd15;
        tb_operand_b = 32'd25;
        
        @(negedge tb_clk); 
        tb_alu_op = ALU_SUB;
        tb_operand_a = 32'd42;
        tb_operand_b = 32'd42;
        
        @(negedge tb_clk); 
        tb_alu_op = ALU_AND;
        tb_operand_a = 32'hFFFF0000;
        tb_operand_b = 32'h00FF0000;
        
        #30; 
        $display("Pipelined ALU Simulation Finished!");
        $finish;
    end

endmodule