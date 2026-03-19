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
    input  logic        S_AXI_RREADY,

    // AXI4 Master (DMA -> HP0)
    output logic [31:0] M_AXI_AWADDR,
    output logic [7:0]  M_AXI_AWLEN,
    output logic [2:0]  M_AXI_AWSIZE,
    output logic [1:0]  M_AXI_AWBURST,
    output logic        M_AXI_AWLOCK,
    output logic [3:0]  M_AXI_AWCACHE,
    output logic [2:0]  M_AXI_AWPROT,
    output logic        M_AXI_AWVALID,
    input  logic        M_AXI_AWREADY,

    output logic [31:0] M_AXI_WDATA,
    output logic [3:0]  M_AXI_WSTRB,
    output logic        M_AXI_WLAST,
    output logic        M_AXI_WVALID,
    input  logic        M_AXI_WREADY,

    input  logic [1:0]  M_AXI_BRESP,
    input  logic        M_AXI_BVALID,
    output logic        M_AXI_BREADY,

    output logic [31:0] M_AXI_ARADDR,
    output logic [7:0]  M_AXI_ARLEN,
    output logic [2:0]  M_AXI_ARSIZE,
    output logic [1:0]  M_AXI_ARBURST,
    output logic        M_AXI_ARLOCK,
    output logic [3:0]  M_AXI_ARCACHE,
    output logic [2:0]  M_AXI_ARPROT,
    output logic        M_AXI_ARVALID,
    input  logic        M_AXI_ARREADY,

    input  logic [31:0] M_AXI_RDATA,
    input  logic [1:0]  M_AXI_RRESP,
    input  logic        M_AXI_RLAST,
    input  logic        M_AXI_RVALID,
    output logic        M_AXI_RREADY
);

    localparam IMEM_DEPTH = 1024;
    localparam DMEM_DEPTH = 4096;
    localparam DMEM_ADDR_W = 12;

    logic [31:0] imem [0:IMEM_DEPTH-1];

    logic gpu_start_run;

    logic [31:0] dma_src_addr_reg;
    logic [31:0] dma_dst_addr_reg;
    logic [11:0] dma_len_reg;
    logic        dma_dir_reg;
    logic        dma_start_pulse;
    logic        dma_busy;

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

    // 解码地址区域
    logic        axi_wr_valid;
    logic [15:0] axi_wr_addr;
    assign axi_wr_valid = axi_wready_reg && S_AXI_WVALID && axi_awready_reg && S_AXI_AWVALID;
    assign axi_wr_addr  = axi_awaddr_reg[15:0];

    // AXI写DMEM时的index和lane
    logic [DMEM_ADDR_W-1:0] axi_dmem_idx;
    logic [1:0]             axi_dmem_lane;
    assign axi_dmem_idx  = (axi_wr_addr - 16'h2000) >> 4;
    assign axi_dmem_lane = axi_awaddr_reg[3:2];

    // CTRL寄存器
    always_ff @(posedge S_AXI_ACLK or negedge S_AXI_ARESETN) begin
        if (!S_AXI_ARESETN) begin
            gpu_start_run <= 1'b0;
        end else if (axi_wr_valid && axi_wr_addr == 16'h0000) begin
            gpu_start_run <= S_AXI_WDATA[0];
        end
    end

    // DMA寄存器
    always_ff @(posedge S_AXI_ACLK or negedge S_AXI_ARESETN) begin
        if (!S_AXI_ARESETN) begin
            dma_src_addr_reg <= 32'h0;
            dma_dst_addr_reg <= 32'h0;
            dma_len_reg      <= 12'h0;
            dma_dir_reg      <= 1'b0;
            dma_start_pulse  <= 1'b0;
        end else begin
            dma_start_pulse <= 1'b0;
            if (axi_wr_valid) begin
                case (axi_wr_addr)
                    16'h0008: dma_src_addr_reg <= S_AXI_WDATA;
                    16'h000C: dma_dst_addr_reg <= S_AXI_WDATA;
                    16'h0010: dma_len_reg      <= S_AXI_WDATA[11:0];
                    16'h0014: begin
                        dma_start_pulse <= S_AXI_WDATA[0];
                        dma_dir_reg     <= S_AXI_WDATA[1];
                    end
                    default: ;
                endcase
            end
        end
    end

    always_ff @(posedge S_AXI_ACLK) begin
        if (axi_wr_valid && axi_wr_addr >= 16'h0100 && axi_wr_addr <= 16'h10FF) begin
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

    assign gpu_imem_data = imem[gpu_imem_addr[9:0]];

    logic [11:0]  dma_dmem_addr;
    logic [3:0]   dma_dmem_we;
    logic [127:0] dma_dmem_wdata;
    logic [127:0] dma_dmem_rdata_wire;
    logic         dma_dmem_active;

    gpu_dma_ctrl u_dma (
        .clk            (S_AXI_ACLK),
        .rst_n          (S_AXI_ARESETN),
        .dma_src_addr   (dma_src_addr_reg),
        .dma_dst_addr   (dma_dst_addr_reg),
        .dma_len        (dma_len_reg),
        .dma_start      (dma_start_pulse),
        .dma_dir        (dma_dir_reg),
        .dma_busy       (dma_busy),
        .dmem_addr      (dma_dmem_addr),
        .dmem_we        (dma_dmem_we),
        .dmem_wdata     (dma_dmem_wdata),
        .dmem_rdata     (dma_dmem_rdata_wire),
        .dmem_active    (dma_dmem_active),
        .M_AXI_AWADDR   (M_AXI_AWADDR),
        .M_AXI_AWLEN    (M_AXI_AWLEN),
        .M_AXI_AWSIZE   (M_AXI_AWSIZE),
        .M_AXI_AWBURST  (M_AXI_AWBURST),
        .M_AXI_AWLOCK   (M_AXI_AWLOCK),
        .M_AXI_AWCACHE  (M_AXI_AWCACHE),
        .M_AXI_AWPROT   (M_AXI_AWPROT),
        .M_AXI_AWVALID  (M_AXI_AWVALID),
        .M_AXI_AWREADY  (M_AXI_AWREADY),
        .M_AXI_WDATA    (M_AXI_WDATA),
        .M_AXI_WSTRB    (M_AXI_WSTRB),
        .M_AXI_WLAST    (M_AXI_WLAST),
        .M_AXI_WVALID   (M_AXI_WVALID),
        .M_AXI_WREADY   (M_AXI_WREADY),
        .M_AXI_BRESP    (M_AXI_BRESP),
        .M_AXI_BVALID   (M_AXI_BVALID),
        .M_AXI_BREADY   (M_AXI_BREADY),
        .M_AXI_ARADDR   (M_AXI_ARADDR),
        .M_AXI_ARLEN    (M_AXI_ARLEN),
        .M_AXI_ARSIZE   (M_AXI_ARSIZE),
        .M_AXI_ARBURST  (M_AXI_ARBURST),
        .M_AXI_ARLOCK   (M_AXI_ARLOCK),
        .M_AXI_ARCACHE  (M_AXI_ARCACHE),
        .M_AXI_ARPROT   (M_AXI_ARPROT),
        .M_AXI_ARVALID  (M_AXI_ARVALID),
        .M_AXI_ARREADY  (M_AXI_ARREADY),
        .M_AXI_RDATA    (M_AXI_RDATA),
        .M_AXI_RRESP    (M_AXI_RRESP),
        .M_AXI_RLAST    (M_AXI_RLAST),
        .M_AXI_RVALID   (M_AXI_RVALID),
        .M_AXI_RREADY   (M_AXI_RREADY)
    );

    logic axi_wr_dmem_bank0, axi_wr_dmem_bank1, axi_wr_dmem_bank2, axi_wr_dmem_bank3;
    assign axi_wr_dmem_bank0 = axi_wr_valid && (axi_wr_addr >= 16'h2000) && (axi_dmem_lane == 2'd0);
    assign axi_wr_dmem_bank1 = axi_wr_valid && (axi_wr_addr >= 16'h2000) && (axi_dmem_lane == 2'd1);
    assign axi_wr_dmem_bank2 = axi_wr_valid && (axi_wr_addr >= 16'h2000) && (axi_dmem_lane == 2'd2);
    assign axi_wr_dmem_bank3 = axi_wr_valid && (axi_wr_addr >= 16'h2000) && (axi_dmem_lane == 2'd3);

    logic [DMEM_ADDR_W-1:0] gpu_dmem_idx;
    assign gpu_dmem_idx = gpu_dmem_addr[DMEM_ADDR_W-1:0];

    logic [31:0] dmem_rd_bank0, dmem_rd_bank1, dmem_rd_bank2, dmem_rd_bank3;
    logic [DMEM_ADDR_W-1:0] axi_rd_dmem_idx;
    logic [1:0]             axi_rd_dmem_lane;

    assign axi_rd_dmem_idx  = (axi_araddr_reg[15:0] - 16'h2000) >> 4;
    assign axi_rd_dmem_lane = axi_araddr_reg[3:2];

    // Port B mux: DMA vs AXI-Lite
    logic [DMEM_ADDR_W-1:0] portb_addr_bank0, portb_addr_bank1, portb_addr_bank2, portb_addr_bank3;
    logic                   portb_we_bank0, portb_we_bank1, portb_we_bank2, portb_we_bank3;
    logic [31:0]            portb_din_bank0, portb_din_bank1, portb_din_bank2, portb_din_bank3;

    always_comb begin
        if (dma_dmem_active) begin
            portb_addr_bank0 = dma_dmem_addr;
            portb_we_bank0   = dma_dmem_we[0];
            portb_din_bank0  = dma_dmem_wdata[31:0];
        end else if (axi_wr_dmem_bank0) begin
            portb_addr_bank0 = axi_dmem_idx;
            portb_we_bank0   = 1'b1;
            portb_din_bank0  = S_AXI_WDATA;
        end else begin
            portb_addr_bank0 = axi_rd_dmem_idx;
            portb_we_bank0   = 1'b0;
            portb_din_bank0  = 32'h0;
        end
    end

    always_comb begin
        if (dma_dmem_active) begin
            portb_addr_bank1 = dma_dmem_addr;
            portb_we_bank1   = dma_dmem_we[1];
            portb_din_bank1  = dma_dmem_wdata[63:32];
        end else if (axi_wr_dmem_bank1) begin
            portb_addr_bank1 = axi_dmem_idx;
            portb_we_bank1   = 1'b1;
            portb_din_bank1  = S_AXI_WDATA;
        end else begin
            portb_addr_bank1 = axi_rd_dmem_idx;
            portb_we_bank1   = 1'b0;
            portb_din_bank1  = 32'h0;
        end
    end

    always_comb begin
        if (dma_dmem_active) begin
            portb_addr_bank2 = dma_dmem_addr;
            portb_we_bank2   = dma_dmem_we[2];
            portb_din_bank2  = dma_dmem_wdata[95:64];
        end else if (axi_wr_dmem_bank2) begin
            portb_addr_bank2 = axi_dmem_idx;
            portb_we_bank2   = 1'b1;
            portb_din_bank2  = S_AXI_WDATA;
        end else begin
            portb_addr_bank2 = axi_rd_dmem_idx;
            portb_we_bank2   = 1'b0;
            portb_din_bank2  = 32'h0;
        end
    end

    always_comb begin
        if (dma_dmem_active) begin
            portb_addr_bank3 = dma_dmem_addr;
            portb_we_bank3   = dma_dmem_we[3];
            portb_din_bank3  = dma_dmem_wdata[127:96];
        end else if (axi_wr_dmem_bank3) begin
            portb_addr_bank3 = axi_dmem_idx;
            portb_we_bank3   = 1'b1;
            portb_din_bank3  = S_AXI_WDATA;
        end else begin
            portb_addr_bank3 = axi_rd_dmem_idx;
            portb_we_bank3   = 1'b0;
            portb_din_bank3  = 32'h0;
        end
    end

    gpu_tdp_bram #(.ADDR_WIDTH(DMEM_ADDR_W), .DATA_WIDTH(32)) u_bank0 (
        .clk(S_AXI_ACLK),
        .we_a(gpu_dmem_we[0]), .addr_a(gpu_dmem_idx),
        .din_a(gpu_dmem_wdata[31:0]), .dout_a(gpu_dmem_rdata[31:0]),
        .we_b(portb_we_bank0), .addr_b(portb_addr_bank0),
        .din_b(portb_din_bank0), .dout_b(dmem_rd_bank0)
    );

    gpu_tdp_bram #(.ADDR_WIDTH(DMEM_ADDR_W), .DATA_WIDTH(32)) u_bank1 (
        .clk(S_AXI_ACLK),
        .we_a(gpu_dmem_we[1]), .addr_a(gpu_dmem_idx),
        .din_a(gpu_dmem_wdata[63:32]), .dout_a(gpu_dmem_rdata[63:32]),
        .we_b(portb_we_bank1), .addr_b(portb_addr_bank1),
        .din_b(portb_din_bank1), .dout_b(dmem_rd_bank1)
    );

    gpu_tdp_bram #(.ADDR_WIDTH(DMEM_ADDR_W), .DATA_WIDTH(32)) u_bank2 (
        .clk(S_AXI_ACLK),
        .we_a(gpu_dmem_we[2]), .addr_a(gpu_dmem_idx),
        .din_a(gpu_dmem_wdata[95:64]), .dout_a(gpu_dmem_rdata[95:64]),
        .we_b(portb_we_bank2), .addr_b(portb_addr_bank2),
        .din_b(portb_din_bank2), .dout_b(dmem_rd_bank2)
    );

    gpu_tdp_bram #(.ADDR_WIDTH(DMEM_ADDR_W), .DATA_WIDTH(32)) u_bank3 (
        .clk(S_AXI_ACLK),
        .we_a(gpu_dmem_we[3]), .addr_a(gpu_dmem_idx),
        .din_a(gpu_dmem_wdata[127:96]), .dout_a(gpu_dmem_rdata[127:96]),
        .we_b(portb_we_bank3), .addr_b(portb_addr_bank3),
        .din_b(portb_din_bank3), .dout_b(dmem_rd_bank3)
    );

    assign dma_dmem_rdata_wire = {dmem_rd_bank3, dmem_rd_bank2, dmem_rd_bank1, dmem_rd_bank0};

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
                        else if (axi_rd_addr == 16'h0008)
                            axi_rdata_reg <= dma_src_addr_reg;
                        else if (axi_rd_addr == 16'h000C)
                            axi_rdata_reg <= dma_dst_addr_reg;
                        else if (axi_rd_addr == 16'h0010)
                            axi_rdata_reg <= {20'h0, dma_len_reg};
                        else if (axi_rd_addr == 16'h0014)
                            axi_rdata_reg <= {30'h0, dma_dir_reg, 1'b0};
                        else if (axi_rd_addr == 16'h0018)
                            axi_rdata_reg <= {31'h0, dma_busy};
                        else if (axi_rd_addr >= 16'h0100 && axi_rd_addr <= 16'h10FF)
                            axi_rdata_reg <= imem[(axi_rd_addr - 16'h0100) >> 2];
                        else if (axi_rd_addr >= 16'h2000)
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
