`timescale 1ns / 1ps

module gpu_soc_wrapper (
    input  logic        S_AXI_ACLK,
    input  logic        S_AXI_ARESETN,
    
    input  logic [31:0] S_AXI_AWADDR,
    input  logic        S_AXI_AWVALID,
    output logic        S_AXI_AWREADY,

    input  logic [31:0] S_AXI_WDATA,
    input  logic [3:0]  S_AXI_WSTRB,
    input  logic        S_AXI_WVALID,
    output logic        S_AXI_WREADY,

    output logic [1:0]  S_AXI_BRESP,
    output logic        S_AXI_BVALID,
    input  logic        S_AXI_BREADY,
    
    input  logic [31:0] S_AXI_ARADDR,
    input  logic        S_AXI_ARVALID,
    output logic        S_AXI_ARREADY,

    output logic [31:0] S_AXI_RDATA,
    output logic [1:0]  S_AXI_RRESP,
    output logic        S_AXI_RVALID,
    input  logic        S_AXI_RREADY
);

    logic [31:0] imem [0:15]; // 16深度的指令内存
    
    logic [127:0] dmem [0:255];
    initial begin
        for (int i=0; i<256; i++) dmem[i] = 128'h0;
        dmem[10] = {32'd100, 32'd30, 32'd100, 32'd10}; 
        dmem[14] = {32'd100, 32'd100, 32'd100, 32'd100}; 
    end

    logic axi_awready_reg, axi_wready_reg, axi_bvalid_reg;
    assign S_AXI_AWREADY = axi_awready_reg;
    assign S_AXI_WREADY  = axi_wready_reg;
    assign S_AXI_BVALID  = axi_bvalid_reg;
    assign S_AXI_BRESP   = 2'b00; // OKAY

    logic aw_en;
    always_ff @(posedge S_AXI_ACLK or negedge S_AXI_ARESETN) begin
        if (!S_AXI_ARESETN) begin
            axi_awready_reg <= 1'b0; axi_wready_reg <= 1'b0; axi_bvalid_reg <= 1'b0; aw_en <= 1'b1;
        end else begin
            // 接收地址
            if (~axi_awready_reg && S_AXI_AWVALID && S_AXI_WVALID && aw_en) begin
                axi_awready_reg <= 1'b1; aw_en <= 1'b0;
            end else if (S_AXI_BREADY && axi_bvalid_reg) begin
                axi_awready_reg <= 1'b0; aw_en <= 1'b1;
            end else begin
                axi_awready_reg <= 1'b0;
            end
            
            // 接收数据
            if (~axi_wready_reg && S_AXI_WVALID && S_AXI_AWVALID && aw_en) begin
                axi_wready_reg <= 1'b1;
            end else begin
                axi_wready_reg <= 1'b0;
            end
            
            // 发送响应
            if (axi_awready_reg && S_AXI_AWVALID && axi_wready_reg && S_AXI_WVALID && ~axi_bvalid_reg) begin
                axi_bvalid_reg <= 1'b1;
            end else if (axi_bvalid_reg && S_AXI_BREADY) begin
                axi_bvalid_reg <= 1'b0;
            end
        end
    end

    assign S_AXI_ARREADY = 1'b1;
    assign S_AXI_RVALID  = S_AXI_ARVALID;
    assign S_AXI_RRESP   = 2'b00;
    assign S_AXI_RDATA   = 32'h0;

    logic gpu_start_run;
    
    always_ff @(posedge S_AXI_ACLK) begin
        if (!S_AXI_ARESETN) begin
            gpu_start_run <= 1'b0;
        end else begin
            if (axi_wready_reg && S_AXI_WVALID && axi_awready_reg && S_AXI_AWVALID) begin
                if (S_AXI_AWADDR[11:0] == 12'h000) begin
                    gpu_start_run <= S_AXI_WDATA[0]; 
                end
                else if (S_AXI_AWADDR[11:0] >= 12'h100 && S_AXI_AWADDR[11:0] <= 12'h13F) begin
                    imem[(S_AXI_AWADDR[11:0] - 12'h100) >> 2] <= S_AXI_WDATA;
                end
            end
        end
    end

    logic [31:0] gpu_imem_addr;
    logic [31:0] gpu_imem_data;
    logic        gpu_dmem_re;
    logic [3:0]  gpu_dmem_we;
    logic [31:0] gpu_dmem_addr;
    logic [127:0] gpu_dmem_wdata;
    logic [127:0] gpu_dmem_rdata;
    logic [3:0]  gpu_flag_zero;

    assign gpu_imem_data = imem[gpu_imem_addr[3:0]];

    always_ff @(posedge S_AXI_ACLK) begin
        if (gpu_dmem_re) gpu_dmem_rdata <= dmem[gpu_dmem_addr[7:0]];
        
        for (int i=0; i<4; i++) begin
            if (gpu_dmem_we[i]) begin
                dmem[gpu_dmem_addr[7:0]][i*32 +: 32] <= gpu_dmem_wdata[i*32 +: 32];
            end
        end
    end

    gpu_core_top u_gpu_core (
        .clk             (S_AXI_ACLK),
        .rst_n           (S_AXI_ARESETN & gpu_start_run), 
        
        .out_imem_addr   (gpu_imem_addr),
        .in_imem_data    (gpu_imem_data),
        
        .out_dmem_re     (gpu_dmem_re),
        .out_dmem_we     (gpu_dmem_we),
        .out_dmem_addr   (gpu_dmem_addr),
        .out_dmem_wdata  (gpu_dmem_wdata),
        .in_dmem_rdata   (gpu_dmem_rdata),
        
        .out_flag_zero   (gpu_flag_zero)
    );

endmodule