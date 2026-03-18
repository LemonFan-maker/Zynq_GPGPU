`timescale 1ns / 1ps

module gpu_regfile (
    input  logic        clk,
    
    input  logic        we,
    input  logic [4:0]  waddr,
    input  logic [31:0] wdata,
    
    // 读端口 1 (Read Port 1)
    input  logic [4:0]  raddr1,    // Read Address 1
    output logic [31:0] rdata1,    // Read Data 1
    
    // 读端口 2 (Read Port 2)
    input  logic [4:0]  raddr2,    // Read Address 2
    output logic [31:0] rdata2,    // Read Data 2

    // 读端口 3 (Read Port 3, MAC 累加器读)
    input  logic [4:0]  raddr3,
    output logic [31:0] rdata3
);

    logic [31:0] regs [31:0];

    assign rdata1 = (raddr1 == 5'b0) ? 32'h0 : regs[raddr1];
    assign rdata2 = (raddr2 == 5'b0) ? 32'h0 : regs[raddr2];
    assign rdata3 = (raddr3 == 5'b0) ? 32'h0 : regs[raddr3];

    always_ff @(posedge clk) begin
        if (we && (waddr != 5'b0)) begin
            regs[waddr] <= wdata;
        end
    end

    // 仿真时初始化所有寄存器为0，防止出现未知态
    initial begin
        for (int i = 0; i < 32; i++) begin
            regs[i] = 32'h0;
        end
    end

endmodule