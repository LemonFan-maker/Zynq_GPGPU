`timescale 1ns / 1ps

module gpu_regfile (
    input  logic        clk,
    input  logic        we,
    input  logic [4:0]  waddr,
    input  logic [31:0] wdata,
    input  logic [4:0]  raddr1,
    output logic [31:0] rdata1,
    input  logic [4:0]  raddr2,
    output logic [31:0] rdata2
);
    logic [31:0] regs [0:31];
    
    always_ff @(posedge clk) begin
        // r0永远为0，不能被写入
        if (we && waddr != 5'd0) begin
            regs[waddr] <= wdata;
        end
    end
    
    assign rdata1 = (raddr1 == 5'd0) ? 32'h0 : regs[raddr1];
    assign rdata2 = (raddr2 == 5'd0) ? 32'h0 : regs[raddr2];

endmodule