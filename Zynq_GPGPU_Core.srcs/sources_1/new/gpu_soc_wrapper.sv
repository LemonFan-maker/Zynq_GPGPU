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

    localparam IMEM_DEPTH = 256;    // 256条指令
    localparam DMEM_DEPTH = 4096;   // 4096 entries × 4 lanes × 32-bit = 64KB
    localparam DMEM_ADDR_W = 12;    // log2(4096)

    logic [31:0] imem [0:IMEM_DEPTH-1];

    logic gpu_start_run;

    logic axi_awready_reg, axi_wready_reg, axi_bvalid_reg;
    logic aw_en;
    logic [31:0] axi_awaddr_reg;

    assign S_AXI_AWREADY = axi_awready_reg;
    assign S_AXI_WREADY  = axi_wready_reg;
    assign S_AXI_BVALID  = axi_bvalid_reg;
    assign S_AXI_BRESP   = 2'b00;

    always_ff @(posedge S_AXI_ACLK or negedge S_AXI_ARESETN) begin
        if (!S_AXI_ARESETN) begin
            axi_awready_reg <= 1'b0;
            axi_wready_reg  <= 1'b0;
            axi_bvalid_reg  <= 1'b0;
            aw_en           <= 1'b1;
            axi_awaddr_reg  <= 32'h0;
        end else begin
            if (~axi_awready_reg && S_AXI_AWVALID && S_AXI_WVALID && aw_en) begin
                axi_awready_reg <= 1'b1;
                aw_en           <= 1'b0;
                axi_awaddr_reg  <= S_AXI_AWADDR;
            end else if (S_AXI_BREADY && axi_bvalid_reg) begin
                axi_awready_reg <= 1'b0;
                aw_en           <= 1'b1;
            end else begin
                axi_awready_reg <= 1'b0;
            end

            if (~axi_wready_reg && S_AXI_WVALID && S_AXI_AWVALID && aw_en) begin
                axi_wready_reg <= 1'b1;
            end else begin
                axi_wready_reg <= 1'b0;
            end

            if (axi_awready_reg && S_AXI_AWVALID && axi_wready_reg && S_AXI_WVALID && ~axi_bvalid_reg) begin
                axi_bvalid_reg <= 1'b1;
            end else if (axi_bvalid_reg && S_AXI_BREADY) begin
                axi_bvalid_reg <= 1'b0;
            end
        end
    end

    // 地址映射 (16-bit 偏移):
    //   0x0000        : CTRL  (bit0 = start)
    //   0x0004        : STATUS
    //   0x0100-0x04FF : IMEM  (256 × 4 bytes)
    //   0x1000-0x8FFF : DMEM  (2048 entries × 16 bytes)
    logic        axi_wr_valid;
    logic [15:0] axi_wr_addr;
    assign axi_wr_valid = axi_wready_reg && S_AXI_WVALID && axi_awready_reg && S_AXI_AWVALID;
    assign axi_wr_addr  = axi_awaddr_reg[15:0];

    // AXI写DMEM时的index和lane
    // entry index: addr[15:4], lane: addr[3:2]
    // DMEM基地址0x1000, 所以entry = (addr - 0x1000) >> 4 = addr[15:4] - 0x100
    logic [DMEM_ADDR_W-1:0] axi_dmem_idx;
    logic [1:0]             axi_dmem_lane;
    assign axi_dmem_idx  = (axi_wr_addr - 16'h1000) >> 4;
    assign axi_dmem_lane = axi_awaddr_reg[3:2];

    // CTRL寄存器
    always_ff @(posedge S_AXI_ACLK or negedge S_AXI_ARESETN) begin
        if (!S_AXI_ARESETN) begin
            gpu_start_run <= 1'b0;
        end else if (axi_wr_valid && axi_wr_addr == 16'h0000) begin
            gpu_start_run <= S_AXI_WDATA[0];
        end
    end

    // IMEM写 (只有AXI写, GPU只读)
    always_ff @(posedge S_AXI_ACLK) begin
        if (axi_wr_valid && axi_wr_addr >= 16'h0100 && axi_wr_addr <= 16'h04FF) begin
            imem[(axi_wr_addr - 16'h0100) >> 2] <= S_AXI_WDATA;
        end
    end

    logic [31:0]  gpu_imem_addr;
    logic [31:0]  gpu_imem_data;
    logic         gpu_dmem_re;
    logic [3:0]   gpu_dmem_we;
    logic [31:0]  gpu_dmem_addr;
    logic [127:0] gpu_dmem_wdata;
    logic [127:0] gpu_dmem_rdata;
    logic [3:0]   gpu_flag_zero;
    logic         gpu_done;

    // IMEM: GPU组合读
    assign gpu_imem_data = imem[gpu_imem_addr[7:0]];

    logic axi_wr_dmem_bank0, axi_wr_dmem_bank1, axi_wr_dmem_bank2, axi_wr_dmem_bank3;
    assign axi_wr_dmem_bank0 = axi_wr_valid && (axi_wr_addr >= 16'h1000) && (axi_dmem_lane == 2'd0);
    assign axi_wr_dmem_bank1 = axi_wr_valid && (axi_wr_addr >= 16'h1000) && (axi_dmem_lane == 2'd1);
    assign axi_wr_dmem_bank2 = axi_wr_valid && (axi_wr_addr >= 16'h1000) && (axi_dmem_lane == 2'd2);
    assign axi_wr_dmem_bank3 = axi_wr_valid && (axi_wr_addr >= 16'h1000) && (axi_dmem_lane == 2'd3);

    logic [DMEM_ADDR_W-1:0] gpu_dmem_idx;
    assign gpu_dmem_idx = gpu_dmem_addr[DMEM_ADDR_W-1:0];

    logic [31:0] dmem_rd_bank0, dmem_rd_bank1, dmem_rd_bank2, dmem_rd_bank3;
    logic [DMEM_ADDR_W-1:0] axi_rd_dmem_idx;
    logic [1:0]             axi_rd_dmem_lane;

    assign axi_rd_dmem_idx  = (axi_araddr_reg[15:0] - 16'h1000) >> 4;
    assign axi_rd_dmem_lane = axi_araddr_reg[3:2];

    gpu_tdp_bram #(.ADDR_WIDTH(DMEM_ADDR_W), .DATA_WIDTH(32)) u_bank0 (
        .clk(S_AXI_ACLK),
        .we_a(gpu_dmem_we[0]), .addr_a(gpu_dmem_idx),
        .din_a(gpu_dmem_wdata[31:0]), .dout_a(gpu_dmem_rdata[31:0]),
        .we_b(axi_wr_dmem_bank0), .addr_b(axi_wr_dmem_bank0 ? axi_dmem_idx : axi_rd_dmem_idx),
        .din_b(S_AXI_WDATA), .dout_b(dmem_rd_bank0)
    );

    gpu_tdp_bram #(.ADDR_WIDTH(DMEM_ADDR_W), .DATA_WIDTH(32)) u_bank1 (
        .clk(S_AXI_ACLK),
        .we_a(gpu_dmem_we[1]), .addr_a(gpu_dmem_idx),
        .din_a(gpu_dmem_wdata[63:32]), .dout_a(gpu_dmem_rdata[63:32]),
        .we_b(axi_wr_dmem_bank1), .addr_b(axi_wr_dmem_bank1 ? axi_dmem_idx : axi_rd_dmem_idx),
        .din_b(S_AXI_WDATA), .dout_b(dmem_rd_bank1)
    );

    gpu_tdp_bram #(.ADDR_WIDTH(DMEM_ADDR_W), .DATA_WIDTH(32)) u_bank2 (
        .clk(S_AXI_ACLK),
        .we_a(gpu_dmem_we[2]), .addr_a(gpu_dmem_idx),
        .din_a(gpu_dmem_wdata[95:64]), .dout_a(gpu_dmem_rdata[95:64]),
        .we_b(axi_wr_dmem_bank2), .addr_b(axi_wr_dmem_bank2 ? axi_dmem_idx : axi_rd_dmem_idx),
        .din_b(S_AXI_WDATA), .dout_b(dmem_rd_bank2)
    );

    gpu_tdp_bram #(.ADDR_WIDTH(DMEM_ADDR_W), .DATA_WIDTH(32)) u_bank3 (
        .clk(S_AXI_ACLK),
        .we_a(gpu_dmem_we[3]), .addr_a(gpu_dmem_idx),
        .din_a(gpu_dmem_wdata[127:96]), .dout_a(gpu_dmem_rdata[127:96]),
        .we_b(axi_wr_dmem_bank3), .addr_b(axi_wr_dmem_bank3 ? axi_dmem_idx : axi_rd_dmem_idx),
        .din_b(S_AXI_WDATA), .dout_b(dmem_rd_bank3)
    );

    logic        axi_arready_reg;
    logic        axi_rvalid_reg;
    logic [31:0] axi_rdata_reg;
    logic [31:0] axi_araddr_reg;

    assign S_AXI_ARREADY = axi_arready_reg;
    assign S_AXI_RVALID  = axi_rvalid_reg;
    assign S_AXI_RDATA   = axi_rdata_reg;
    assign S_AXI_RRESP   = 2'b00;

    logic [31:0] dmem_rd_selected;
    always_comb begin
        case (axi_rd_dmem_lane)
            2'd0: dmem_rd_selected = dmem_rd_bank0;
            2'd1: dmem_rd_selected = dmem_rd_bank1;
            2'd2: dmem_rd_selected = dmem_rd_bank2;
            2'd3: dmem_rd_selected = dmem_rd_bank3;
        endcase
    end

    // AXI读状态机
    typedef enum logic [1:0] {
        RD_IDLE   = 2'b00,
        RD_WAIT   = 2'b01,
        RD_RESP   = 2'b10
    } rd_state_t;
    rd_state_t rd_state;

    logic [15:0] axi_rd_addr;
    assign axi_rd_addr = axi_araddr_reg[15:0];

    always_ff @(posedge S_AXI_ACLK or negedge S_AXI_ARESETN) begin
        if (!S_AXI_ARESETN) begin
            rd_state        <= RD_IDLE;
            axi_arready_reg <= 1'b0;
            axi_rvalid_reg  <= 1'b0;
            axi_rdata_reg   <= 32'h0;
            axi_araddr_reg  <= 32'h0;
        end else begin
            case (rd_state)
                RD_IDLE: begin
                    axi_rvalid_reg <= 1'b0;
                    if (S_AXI_ARVALID) begin
                        axi_arready_reg <= 1'b1;
                        axi_araddr_reg  <= S_AXI_ARADDR;
                        rd_state        <= RD_WAIT;
                    end
                end

                RD_WAIT: begin
                    axi_arready_reg <= 1'b0;
                    rd_state        <= RD_RESP;
                end

                RD_RESP: begin
                    if (!axi_rvalid_reg) begin
                        axi_rvalid_reg <= 1'b1;

                        if (axi_rd_addr == 16'h0000)
                            axi_rdata_reg <= {31'b0, gpu_start_run};
                        else if (axi_rd_addr == 16'h0004)
                            axi_rdata_reg <= {31'b0, gpu_done};
                        else if (axi_rd_addr >= 16'h0100 && axi_rd_addr <= 16'h04FF)
                            axi_rdata_reg <= imem[(axi_rd_addr - 16'h0100) >> 2];
                        else if (axi_rd_addr >= 16'h1000)
                            axi_rdata_reg <= dmem_rd_selected;
                        else
                            axi_rdata_reg <= 32'h0;
                    end else if (S_AXI_RREADY) begin
                        axi_rvalid_reg <= 1'b0;
                        rd_state       <= RD_IDLE;
                    end
                end

                default: rd_state <= RD_IDLE;
            endcase
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

        .out_flag_zero   (gpu_flag_zero),

        .gpu_done        (gpu_done)
    );

endmodule
