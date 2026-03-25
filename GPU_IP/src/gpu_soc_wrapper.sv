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

    localparam int IMEM_DEPTH = 1024;
    localparam int DMEM_DEPTH = 4096;
    localparam int DMEM_ADDR_W = 12;
    localparam int NUM_LANES = 16;
    localparam int DMEM_DATA_W = NUM_LANES * 32;
    localparam int LANE_BITS = (NUM_LANES <= 1) ? 1 : $clog2(NUM_LANES);
    localparam int ENTRY_BYTES = NUM_LANES * 4;
    localparam int ENTRY_ADDR_SHIFT = (ENTRY_BYTES <= 1) ? 1 : $clog2(ENTRY_BYTES);
    localparam [31:0] GPU_BUILD_ID = 32'h26032501;

    logic [31:0] imem [0:IMEM_DEPTH-1];

    logic gpu_start_run;
    logic gpu_start_pulse;

    logic [31:0] dma_src_addr_reg;
    logic [31:0] dma_dst_addr_reg;
    logic [11:0] dma_x_size_reg;
    logic [11:0] dma_y_size_reg;
    logic [31:0] dma_stride_reg;
    logic        dma_dir_reg;
    logic        dma_acc_mode_reg;
    logic        dma_im2col_en_reg;
    logic [31:0] dma_im2col_cfg0_reg;
    logic [31:0] dma_im2col_cfg1_reg;
    logic [31:0] dma_im2col_cfg2_reg;
    logic [31:0] dma_im2col_cfg3_reg;
    logic [31:0] dma_im2col_cfg4_reg;
    logic [31:0] dma_im2col_cfg5_reg;
    logic        dma_start_pulse;
    logic        dma_busy;

    logic        acc_clr_pulse;

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

    logic        axi_wr_valid;
    logic [31:0] axi_wr_addr;
    logic [15:0] axi_wr_off;
    logic [DMEM_ADDR_W-1:0] axi_dmem_idx;
    logic [LANE_BITS-1:0]   axi_dmem_lane;
    logic        ctrl_wr_hit;

    assign axi_wr_valid = axi_wready_reg && S_AXI_WVALID && axi_awready_reg && S_AXI_AWVALID;
    assign axi_wr_addr  = axi_awaddr_reg;
    assign axi_wr_off   = axi_wr_addr[15:0];
    assign ctrl_wr_hit  = axi_wr_valid && (axi_wr_off == 16'h0000);
    assign axi_dmem_idx = (axi_wr_off - 16'h2000) >> ENTRY_ADDR_SHIFT;
    assign axi_dmem_lane = axi_wr_off[ENTRY_ADDR_SHIFT-1:2];

    always_ff @(posedge S_AXI_ACLK or negedge S_AXI_ARESETN) begin
        if (!S_AXI_ARESETN) begin
            gpu_start_run   <= 1'b0;
            gpu_start_pulse <= 1'b0;
        end else begin
            gpu_start_pulse <= 1'b0;
            if (ctrl_wr_hit) begin
                gpu_start_run <= S_AXI_WDATA[0];
                if (S_AXI_WDATA[0])
                    gpu_start_pulse <= 1'b1;
            end
        end
    end

    always_ff @(posedge S_AXI_ACLK or negedge S_AXI_ARESETN) begin
        if (!S_AXI_ARESETN) begin
            dma_src_addr_reg    <= 32'h0;
            dma_dst_addr_reg    <= 32'h0;
            dma_x_size_reg      <= 12'h0;
            dma_y_size_reg      <= 12'h0;
            dma_stride_reg      <= 32'h0;
            dma_dir_reg         <= 1'b0;
            dma_acc_mode_reg    <= 1'b0;
            dma_im2col_en_reg   <= 1'b0;
            dma_im2col_cfg0_reg <= 32'h0;
            dma_im2col_cfg1_reg <= 32'h0;
            dma_im2col_cfg2_reg <= 32'h0;
            dma_im2col_cfg3_reg <= 32'h0;
            dma_im2col_cfg4_reg <= 32'h0;
            dma_im2col_cfg5_reg <= 32'h0;
            dma_start_pulse     <= 1'b0;
            acc_clr_pulse       <= 1'b0;
        end else begin
            dma_start_pulse <= 1'b0;
            acc_clr_pulse   <= 1'b0;
            if (axi_wr_valid) begin
                case (axi_wr_addr[15:0])
                    16'h0008: dma_src_addr_reg <= S_AXI_WDATA;
                    16'h000C: dma_dst_addr_reg <= S_AXI_WDATA;
                    16'h0010: dma_x_size_reg   <= S_AXI_WDATA[11:0];
                    16'h0014: dma_y_size_reg   <= S_AXI_WDATA[11:0];
                    16'h0018: dma_stride_reg   <= S_AXI_WDATA;
                    16'h001C: begin
                        dma_start_pulse   <= S_AXI_WDATA[0];
                        dma_dir_reg       <= S_AXI_WDATA[1];
                        dma_acc_mode_reg  <= S_AXI_WDATA[2];
                        dma_im2col_en_reg <= S_AXI_WDATA[3];
                    end
                    16'h0024: acc_clr_pulse       <= S_AXI_WDATA[0];
                    16'h0038: dma_im2col_cfg0_reg <= S_AXI_WDATA;
                    16'h003C: dma_im2col_cfg1_reg <= S_AXI_WDATA;
                    16'h0040: dma_im2col_cfg2_reg <= S_AXI_WDATA;
                    16'h0044: dma_im2col_cfg3_reg <= S_AXI_WDATA;
                    16'h0048: dma_im2col_cfg4_reg <= S_AXI_WDATA;
                    16'h004C: dma_im2col_cfg5_reg <= S_AXI_WDATA;
                    default: ;
                endcase
            end
        end
    end

    always_ff @(posedge S_AXI_ACLK) begin
        if (axi_wr_valid && axi_wr_off >= 16'h0100 && axi_wr_off <= 16'h10FF) begin
            imem[(axi_wr_off - 16'h0100) >> 2] <= S_AXI_WDATA;
        end
    end

    logic [31:0]  gpu_imem_addr;
    logic [31:0]  gpu_imem_data;
    logic         gpu_dmem_re;
    logic [NUM_LANES-1:0]   gpu_dmem_we;
    logic [31:0]  gpu_dmem_addr;
    logic [DMEM_DATA_W-1:0] gpu_dmem_wdata;
    logic [DMEM_DATA_W-1:0] gpu_dmem_rdata;
    logic [NUM_LANES-1:0]   gpu_flag_zero;
    logic         gpu_done;

    logic [31:0] dbg_start_cnt;
    logic [31:0] dbg_done_cnt;
    logic [31:0] dbg_last_pc;
    logic [31:0] dbg_status_flags;
    logic        gpu_done_d;

    assign gpu_imem_data = imem[gpu_imem_addr[9:0]];

    always_ff @(posedge S_AXI_ACLK or negedge S_AXI_ARESETN) begin
        if (!S_AXI_ARESETN) begin
            dbg_start_cnt    <= 32'h0;
            dbg_done_cnt     <= 32'h0;
            dbg_last_pc      <= 32'h0;
            dbg_status_flags <= 32'h0;
            gpu_done_d       <= 1'b0;
        end else begin
            gpu_done_d <= gpu_done;
            dbg_last_pc <= gpu_imem_addr;
            dbg_status_flags <= {28'h0, gpu_start_pulse, gpu_done, dma_busy, gpu_start_run};
            if (gpu_start_pulse)
                dbg_start_cnt <= dbg_start_cnt + 32'd1;
            if (!gpu_done_d && gpu_done)
                dbg_done_cnt <= dbg_done_cnt + 32'd1;
        end
    end

    logic [11:0]  dma_dmem_addr;
    logic [NUM_LANES-1:0]   dma_dmem_we;
    logic [DMEM_DATA_W-1:0] dma_dmem_wdata;
    logic [DMEM_DATA_W-1:0] dma_dmem_rdata_wire;
    logic         dma_dmem_active;

    logic [5:0]   acc_rd_addr;
    logic [DMEM_DATA_W-1:0] acc_rd_data;
    logic [5:0]   dma_acc_rd_addr;

    gpu_dma_ctrl #(
        .NUM_LANES(NUM_LANES)
    ) u_dma (
        .clk            (S_AXI_ACLK),
        .rst_n          (S_AXI_ARESETN),
        .dma_src_addr   (dma_src_addr_reg),
        .dma_dst_addr   (dma_dst_addr_reg),
        .dma_x_size     (dma_x_size_reg),
        .dma_y_size     (dma_y_size_reg),
        .dma_stride     (dma_stride_reg),
        .dma_start      (dma_start_pulse),
        .dma_dir        (dma_dir_reg),
        .dma_acc_mode   (dma_acc_mode_reg),
        .dma_busy       (dma_busy),
        .dmem_addr      (dma_dmem_addr),
        .dmem_we        (dma_dmem_we),
        .dmem_wdata     (dma_dmem_wdata),
        .dmem_rdata     (dma_dmem_rdata_wire),
        .dmem_active    (dma_dmem_active),
        .acc_rd_addr    (dma_acc_rd_addr),
        .acc_rdata      (acc_rd_data),
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

    logic [DMEM_ADDR_W-1:0] gpu_dmem_idx;
    logic [NUM_LANES-1:0]   axi_wr_dmem_bank;
    logic [DMEM_ADDR_W-1:0] axi_rd_dmem_idx;
    logic [LANE_BITS-1:0]   axi_rd_dmem_lane;
    logic [31:0]            dmem_rd_bank [0:NUM_LANES-1];
    logic [DMEM_ADDR_W-1:0] portb_addr_bank [0:NUM_LANES-1];
    logic                   portb_we_bank [0:NUM_LANES-1];
    logic [31:0]            portb_din_bank [0:NUM_LANES-1];
    logic                   axi_arready_reg;
    logic                   axi_rvalid_reg;
    logic [31:0]            axi_rdata_reg;
    logic [31:0]            axi_araddr_reg;
    logic [31:0]            dmem_rd_selected;
    logic [15:0]            axi_rd_off;

    assign gpu_dmem_idx   = gpu_dmem_addr[DMEM_ADDR_W-1:0];
    assign axi_rd_off       = axi_araddr_reg[15:0];
    assign axi_rd_dmem_idx  = (axi_rd_off - 16'h2000) >> ENTRY_ADDR_SHIFT;
    assign axi_rd_dmem_lane = axi_rd_off[ENTRY_ADDR_SHIFT-1:2];

    genvar g_lane;
    generate
        for (g_lane = 0; g_lane < NUM_LANES; g_lane = g_lane + 1) begin : gen_dmem
            assign axi_wr_dmem_bank[g_lane] =
                axi_wr_valid && (axi_wr_off >= 16'h2000) && (axi_dmem_lane == LANE_BITS'(g_lane));

            gpu_tdp_bram #(.ADDR_WIDTH(DMEM_ADDR_W), .DATA_WIDTH(32)) u_bank (
                .clk   (S_AXI_ACLK),
                .we_a  (gpu_dmem_we[g_lane]),
                .addr_a(gpu_dmem_idx),
                .din_a (gpu_dmem_wdata[g_lane*32 +: 32]),
                .dout_a(gpu_dmem_rdata[g_lane*32 +: 32]),
                .we_b  (portb_we_bank[g_lane]),
                .addr_b(portb_addr_bank[g_lane]),
                .din_b (portb_din_bank[g_lane]),
                .dout_b(dmem_rd_bank[g_lane])
            );

            assign dma_dmem_rdata_wire[g_lane*32 +: 32] = dmem_rd_bank[g_lane];
        end
    endgenerate

    always_comb begin
        for (int i = 0; i < NUM_LANES; i++) begin
            if (dma_dmem_active) begin
                portb_addr_bank[i] = dma_dmem_addr;
                portb_we_bank[i]   = dma_dmem_we[i];
                portb_din_bank[i]  = dma_dmem_wdata[i*32 +: 32];
            end else if (axi_wr_dmem_bank[i]) begin
                portb_addr_bank[i] = axi_dmem_idx;
                portb_we_bank[i]   = 1'b1;
                portb_din_bank[i]  = S_AXI_WDATA;
            end else begin
                portb_addr_bank[i] = axi_rd_dmem_idx;
                portb_we_bank[i]   = 1'b0;
                portb_din_bank[i]  = 32'h0;
            end
        end
    end

    assign S_AXI_ARREADY = axi_arready_reg;
    assign S_AXI_RVALID  = axi_rvalid_reg;
    assign S_AXI_RDATA   = axi_rdata_reg;
    assign S_AXI_RRESP   = 2'b00;

    always_comb begin
        dmem_rd_selected = 32'h0;
        if (axi_rd_dmem_lane < NUM_LANES)
            dmem_rd_selected = dmem_rd_bank[axi_rd_dmem_lane];
    end

    typedef enum logic [1:0] {
        RD_IDLE   = 2'b00,
        RD_WAIT   = 2'b01,
        RD_RESP   = 2'b10
    } rd_state_t;
    rd_state_t rd_state;

    logic [31:0] axi_rd_addr;
    assign axi_rd_addr = axi_araddr_reg;

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

                        if (axi_rd_off == 16'h0000)
                            axi_rdata_reg <= {31'b0, gpu_start_run};
                        else if (axi_rd_off == 16'h0004)
                            axi_rdata_reg <= {31'b0, gpu_done};
                        else if (axi_rd_off == 16'h0008)
                            axi_rdata_reg <= dma_src_addr_reg;
                        else if (axi_rd_off == 16'h000C)
                            axi_rdata_reg <= dma_dst_addr_reg;
                        else if (axi_rd_off == 16'h0010)
                            axi_rdata_reg <= {20'h0, dma_x_size_reg};
                        else if (axi_rd_off == 16'h0014)
                            axi_rdata_reg <= {20'h0, dma_y_size_reg};
                        else if (axi_rd_off == 16'h0018)
                            axi_rdata_reg <= dma_stride_reg;
                        else if (axi_rd_off == 16'h001C)
                            axi_rdata_reg <= {28'h0, dma_im2col_en_reg, dma_acc_mode_reg, dma_dir_reg, 1'b0};
                        else if (axi_rd_off == 16'h0020)
                            axi_rdata_reg <= {31'h0, dma_busy};
                        else if (axi_rd_off == 16'h0024)
                            axi_rdata_reg <= 32'h0;
                        else if (axi_rd_off == 16'h0028)
                            axi_rdata_reg <= dbg_start_cnt;
                        else if (axi_rd_off == 16'h002C)
                            axi_rdata_reg <= dbg_done_cnt;
                        else if (axi_rd_off == 16'h0030)
                            axi_rdata_reg <= dbg_last_pc;
                        else if (axi_rd_off == 16'h0034)
                            axi_rdata_reg <= dbg_status_flags;
                        else if (axi_rd_off == 16'h0038)
                            axi_rdata_reg <= dma_im2col_cfg0_reg;
                        else if (axi_rd_off == 16'h003C)
                            axi_rdata_reg <= dma_im2col_cfg1_reg;
                        else if (axi_rd_off == 16'h0040)
                            axi_rdata_reg <= dma_im2col_cfg2_reg;
                        else if (axi_rd_off == 16'h0044)
                            axi_rdata_reg <= dma_im2col_cfg3_reg;
                        else if (axi_rd_off == 16'h0048)
                            axi_rdata_reg <= dma_im2col_cfg4_reg;
                        else if (axi_rd_off == 16'h004C)
                            axi_rdata_reg <= dma_im2col_cfg5_reg;
                        else if (axi_rd_off == 16'h00FC)
                            axi_rdata_reg <= GPU_BUILD_ID;
                        else if (axi_rd_off >= 16'h0100 && axi_rd_off <= 16'h10FF)
                            axi_rdata_reg <= imem[(axi_rd_off - 16'h0100) >> 2];
                        else if (axi_rd_off >= 16'h2000)
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

    gpu_core_top #(
        .NUM_LANES(NUM_LANES)
    ) u_gpu_core (
        .clk             (S_AXI_ACLK),
        .rst_n           (S_AXI_ARESETN),
        .start_pulse     (gpu_start_pulse),

        .out_imem_addr   (gpu_imem_addr),
        .in_imem_data    (gpu_imem_data),

        .out_dmem_re     (gpu_dmem_re),
        .out_dmem_we     (gpu_dmem_we),
        .out_dmem_addr   (gpu_dmem_addr),
        .out_dmem_wdata  (gpu_dmem_wdata),
        .in_dmem_rdata   (gpu_dmem_rdata),

        .out_flag_zero   (gpu_flag_zero),

        .in_acc_clr      (acc_clr_pulse),
        .in_acc_rd_addr  (dma_acc_rd_addr),
        .out_acc_rd_data (acc_rd_data),

        .gpu_done        (gpu_done)
    );

endmodule
