`timescale 1ns / 1ps

module gpu_soc_wrapper #(
    parameter NUM_LANES = 4
)(
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

    // IMEM: 1024条32-bit指令(4KB)
    (* ram_style = "block" *) logic [31:0] imem [0:1023]; 
    
    // DMEM: 1024行，每行4个核心的数据(128-bit 宽)->16KB显存
    (* ram_style = "block" *) logic [(NUM_LANES*32)-1:0] dmem [0:1023];

    logic axi_awready_reg, axi_wready_reg, axi_bvalid_reg;
    logic axi_arready_reg, axi_rvalid_reg;
    logic [31:0] axi_awaddr, axi_araddr;
    logic aw_en;

    assign S_AXI_AWREADY = axi_awready_reg;
    assign S_AXI_WREADY  = axi_wready_reg;
    assign S_AXI_BVALID  = axi_bvalid_reg;
    assign S_AXI_BRESP   = 2'b00;
    assign S_AXI_ARREADY = axi_arready_reg;
    assign S_AXI_RVALID  = axi_rvalid_reg;
    assign S_AXI_RRESP   = 2'b00;

    always_ff @(posedge S_AXI_ACLK or negedge S_AXI_ARESETN) begin
        if (!S_AXI_ARESETN) begin
            axi_awready_reg <= 1'b0; axi_wready_reg <= 1'b0; axi_bvalid_reg <= 1'b0; aw_en <= 1'b1;
            axi_arready_reg <= 1'b0; axi_rvalid_reg <= 1'b0;
        end else begin
            if (~axi_awready_reg && S_AXI_AWVALID && S_AXI_WVALID && aw_en) begin
                axi_awready_reg <= 1'b1; aw_en <= 1'b0; axi_awaddr <= S_AXI_AWADDR;
            end else if (S_AXI_BREADY && axi_bvalid_reg) begin
                axi_awready_reg <= 1'b0; aw_en <= 1'b1;
            end else axi_awready_reg <= 1'b0;
            
            if (~axi_wready_reg && S_AXI_WVALID && S_AXI_AWVALID && aw_en) axi_wready_reg <= 1'b1;
            else axi_wready_reg <= 1'b0;
            
            if (axi_awready_reg && S_AXI_AWVALID && axi_wready_reg && S_AXI_WVALID && ~axi_bvalid_reg) axi_bvalid_reg <= 1'b1;
            else if (axi_bvalid_reg && S_AXI_BREADY) axi_bvalid_reg <= 1'b0;

            if (~axi_arready_reg && S_AXI_ARVALID) begin
                axi_arready_reg <= 1'b1; axi_araddr <= S_AXI_ARADDR;
            end else axi_arready_reg <= 1'b0;

            if (axi_arready_reg && S_AXI_ARVALID && ~axi_rvalid_reg) axi_rvalid_reg <= 1'b1;
            else if (axi_rvalid_reg && S_AXI_RREADY) axi_rvalid_reg <= 1'b0;
        end
    end

    logic slv_reg_wren, slv_reg_rden;
    assign slv_reg_wren = axi_wready_reg && S_AXI_WVALID && axi_awready_reg && S_AXI_AWVALID;
    assign slv_reg_rden = axi_arready_reg && S_AXI_ARVALID && ~axi_rvalid_reg;

    logic gpu_start_run;
    always_ff @(posedge S_AXI_ACLK) begin
        if (!S_AXI_ARESETN) gpu_start_run <= 1'b0;
        else if (slv_reg_wren) begin
            // 0x00000: 控制寄存器
            if (axi_awaddr[19:0] == 20'h00000) 
                gpu_start_run <= S_AXI_WDATA[0]; 
                
            // 0x10000: IMEM (4KB 区间)
            else if (axi_awaddr[19:16] == 4'h1) 
                imem[axi_awaddr[11:2]] <= S_AXI_WDATA;
        end
    end

    logic [31:0] gpu_imem_addr, gpu_imem_data;
    logic gpu_dmem_re;
    logic [NUM_LANES-1:0] gpu_dmem_we;
    logic [31:0] gpu_dmem_addr;
    logic [(NUM_LANES*32)-1:0] gpu_dmem_wdata, gpu_dmem_rdata;
    logic [NUM_LANES-1:0] gpu_flag_zero;

    assign gpu_imem_data = imem[gpu_imem_addr[9:0]];

    always_ff @(posedge S_AXI_ACLK) begin
        // GPU内部读取总线
        if (gpu_dmem_re) gpu_dmem_rdata <= dmem[gpu_dmem_addr[9:0]]; // 仅需9:0以映射1024条目
        
        // 双口内存写入仲裁
        if (gpu_start_run) begin
            // 运行时，GPU的4个核心并行写入显存
            for (int i=0; i<NUM_LANES; i++) begin
                if (gpu_dmem_we[i]) 
                    dmem[gpu_dmem_addr[9:0]][i*32 +: 32] <= gpu_dmem_wdata[i*32 +: 32];
            end
        end else begin
            // 停机待命时，ARM (AXI)接管写入显存
            // 0x20000: DMEM (16KB 区间)
            if (slv_reg_wren && axi_awaddr[19:16] == 4'h2) begin
                dmem[axi_awaddr[13:4]][axi_awaddr[3:2]*32 +: 32] <= S_AXI_WDATA;
            end
        end
    end

    always_ff @(posedge S_AXI_ACLK) begin
        if (slv_reg_rden) begin
            // 仅在0x20000 DMEM区间吐出数据
            if (axi_araddr[19:16] == 4'h2) begin
                S_AXI_RDATA <= dmem[axi_araddr[13:4]][axi_araddr[3:2]*32 +: 32];
            end else begin
                S_AXI_RDATA <= 32'h0;
            end
        end
    end

    gpu_core_top #(
        .NUM_LANES (NUM_LANES)
    ) u_gpu_core (
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