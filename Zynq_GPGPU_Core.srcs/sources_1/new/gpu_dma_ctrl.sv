`timescale 1ns / 1ps

module gpu_dma_ctrl (
    input  logic        clk,
    input  logic        rst_n,

    input  logic [31:0] dma_src_addr,
    input  logic [31:0] dma_dst_addr,
    input  logic [11:0] dma_len,
    input  logic        dma_start,
    input  logic        dma_dir,
    output logic        dma_busy,

    output logic [11:0] dmem_addr,
    output logic [3:0]  dmem_we,
    output logic [127:0] dmem_wdata,
    input  logic [127:0] dmem_rdata,
    output logic        dmem_active,

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

    assign M_AXI_AWSIZE  = 3'b010;
    assign M_AXI_AWBURST = 2'b01;
    assign M_AXI_AWLOCK  = 1'b0;
    assign M_AXI_AWCACHE = 4'b0011;
    assign M_AXI_AWPROT  = 3'b000;
    assign M_AXI_WSTRB   = 4'b1111;

    assign M_AXI_ARSIZE  = 3'b010;
    assign M_AXI_ARBURST = 2'b01;
    assign M_AXI_ARLOCK  = 1'b0;
    assign M_AXI_ARCACHE = 4'b0011;
    assign M_AXI_ARPROT  = 3'b000;

    assign M_AXI_BREADY  = 1'b1;

    typedef enum logic [3:0] {
        S_IDLE,
        S_RD_CALC,
        S_RD_ADDR,
        S_RD_DATA,
        S_RD_DMEM_WR,
        S_WR_DMEM_RD,
        S_WR_DMEM_LATCH,
        S_WR_ADDR,
        S_WR_DATA,
        S_WR_RESP,

        S_DONE
    } state_t;

    state_t state;

    logic [31:0] ddr_addr;
    logic [11:0] dmem_idx;
    logic [11:0] entries_remaining;
    logic        dir_reg;

    logic [127:0] acc_data;
    logic [1:0]   beat_cnt;

    logic [7:0]  burst_len;
    logic [8:0]  burst_beats_left;

    logic [127:0] wr_data_latch;
    logic [1:0]   wr_beat_cnt;

    logic [11:0] dmem_wr_addr;

    function logic [8:0] calc_burst_beats;
        input logic [31:0] addr;
        input logic [11:0] entries;
        logic [11:0] bytes_to_boundary;
        logic [8:0]  beats_to_boundary;
        logic [8:0]  beats_for_entries;
    begin
        bytes_to_boundary = 12'hFFF - addr[11:0] + 1'b1;
        beats_to_boundary = bytes_to_boundary >> 2;
        if (beats_to_boundary == 0) beats_to_boundary = 9'd256;
        beats_for_entries = (entries <= 12'd64) ? {1'b0, entries[5:0], 2'b00} : 9'd256;
        calc_burst_beats = (beats_to_boundary < beats_for_entries) ?
                           beats_to_boundary : beats_for_entries;
        if (calc_burst_beats > 9'd256) calc_burst_beats = 9'd256;
    end
    endfunction

    assign dma_busy    = (state != S_IDLE);
    assign dmem_active = (state != S_IDLE) && (state != S_DONE);

    always_comb begin
        dmem_addr  = (state == S_RD_DMEM_WR) ? dmem_wr_addr : dmem_idx;
        dmem_we    = (state == S_RD_DMEM_WR) ? 4'b1111 : 4'b0000;
        dmem_wdata = acc_data;
    end

    assign M_AXI_ARADDR  = ddr_addr;
    assign M_AXI_ARVALID = (state == S_RD_ADDR);
    assign M_AXI_ARLEN   = burst_len;
    assign M_AXI_RREADY  = (state == S_RD_DATA);

    assign M_AXI_AWADDR  = ddr_addr;
    assign M_AXI_AWVALID = (state == S_WR_ADDR);
    assign M_AXI_AWLEN   = burst_len;

    always_comb begin
        M_AXI_WVALID = (state == S_WR_DATA);
        M_AXI_WLAST  = (state == S_WR_DATA) && (burst_beats_left == 9'd1);
        case (wr_beat_cnt)
            2'd0: M_AXI_WDATA = wr_data_latch[31:0];
            2'd1: M_AXI_WDATA = wr_data_latch[63:32];
            2'd2: M_AXI_WDATA = wr_data_latch[95:64];
            2'd3: M_AXI_WDATA = wr_data_latch[127:96];
        endcase
    end

    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            state             <= S_IDLE;
            ddr_addr          <= 32'h0;
            dmem_idx          <= 12'h0;
            dmem_wr_addr      <= 12'h0;
            entries_remaining <= 12'h0;
            dir_reg           <= 1'b0;
            acc_data          <= 128'h0;
            beat_cnt          <= 2'd0;
            burst_len         <= 8'd0;
            burst_beats_left  <= 9'd0;
            wr_data_latch     <= 128'h0;
            wr_beat_cnt       <= 2'd0;
        end else begin
            case (state)
                S_IDLE: begin
                    if (dma_start && dma_len != 12'd0) begin
                        dir_reg           <= dma_dir;
                        entries_remaining <= dma_len;
                        if (!dma_dir) begin
                            // DDR -> DMEM
                            ddr_addr  <= dma_src_addr;
                            dmem_idx  <= dma_dst_addr[11:0];
                            state     <= S_RD_CALC;
                        end else begin
                            // DMEM -> DDR
                            ddr_addr  <= dma_dst_addr;
                            dmem_idx  <= dma_src_addr[11:0];
                            state     <= S_WR_DMEM_RD;
                        end
                    end
                end

                S_RD_CALC: begin
                    burst_len        <= calc_burst_beats(ddr_addr, entries_remaining) - 9'd1;
                    burst_beats_left <= calc_burst_beats(ddr_addr, entries_remaining);
                    beat_cnt         <= 2'd0;
                    state            <= S_RD_ADDR;
                end

                S_RD_ADDR: begin
                    if (M_AXI_ARREADY) begin
                        state <= S_RD_DATA;
                    end
                end

                S_RD_DATA: begin
                    if (M_AXI_RVALID) begin
                        case (beat_cnt)
                            2'd0: acc_data[31:0]   <= M_AXI_RDATA;
                            2'd1: acc_data[63:32]  <= M_AXI_RDATA;
                            2'd2: acc_data[95:64]  <= M_AXI_RDATA;
                            2'd3: acc_data[127:96] <= M_AXI_RDATA;
                        endcase
                        beat_cnt         <= beat_cnt + 2'd1;
                        burst_beats_left <= burst_beats_left - 9'd1;
                        ddr_addr         <= ddr_addr + 32'd4;

                        if (beat_cnt == 2'd3) begin
                            dmem_wr_addr      <= dmem_idx;
                            dmem_idx          <= dmem_idx + 12'd1;
                            entries_remaining <= entries_remaining - 12'd1;
                            state             <= S_RD_DMEM_WR;
                        end
                    end
                end

                S_RD_DMEM_WR: begin
                    if (entries_remaining == 12'd0) begin
                        state <= S_DONE;
                    end else if (burst_beats_left == 9'd0) begin
                        state <= S_RD_CALC;
                    end else begin
                        state <= S_RD_DATA;
                    end
                end

                S_WR_DMEM_RD: begin
                    state <= S_WR_DMEM_LATCH;
                end

                S_WR_DMEM_LATCH: begin
                    wr_data_latch <= dmem_rdata;
                    wr_beat_cnt   <= 2'd0;
                    burst_len        <= 8'd3;
                    burst_beats_left <= 9'd4;
                    state            <= S_WR_ADDR;
                end

                S_WR_ADDR: begin
                    if (M_AXI_AWREADY) begin
                        state <= S_WR_DATA;
                    end
                end

                S_WR_DATA: begin
                    if (M_AXI_WREADY) begin
                        wr_beat_cnt      <= wr_beat_cnt + 2'd1;
                        burst_beats_left <= burst_beats_left - 9'd1;
                        ddr_addr         <= ddr_addr + 32'd4;

                        if (burst_beats_left == 9'd1) begin
                            state <= S_WR_RESP;
                        end
                    end
                end

                S_WR_RESP: begin
                    if (M_AXI_BVALID) begin
                        dmem_idx          <= dmem_idx + 12'd1;
                        entries_remaining <= entries_remaining - 12'd1;
                        if (entries_remaining == 12'd1) begin
                            state <= S_DONE;
                        end else begin
                            state <= S_WR_DMEM_RD;
                        end
                    end
                end

                S_DONE: begin
                    state <= S_IDLE;
                end

                default: state <= S_IDLE;
            endcase
        end
    end

endmodule
