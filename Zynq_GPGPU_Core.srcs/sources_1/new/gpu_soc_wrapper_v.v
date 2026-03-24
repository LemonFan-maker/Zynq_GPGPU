`timescale 1ns / 1ps

module gpu_soc_wrapper_v (
    (* X_INTERFACE_INFO = "xilinx.com:signal:clock:1.0 S_AXI_ACLK CLK" *)
    (* X_INTERFACE_PARAMETER = "ASSOCIATED_BUSIF S_AXI:M_AXI, ASSOCIATED_RESET S_AXI_ARESETN, FREQ_HZ 75000000" *)
    input  wire        S_AXI_ACLK,

    (* X_INTERFACE_INFO = "xilinx.com:signal:reset:1.0 S_AXI_ARESETN RST" *)
    (* X_INTERFACE_PARAMETER = "POLARITY ACTIVE_LOW" *)
    input  wire        S_AXI_ARESETN,

    input  wire [31:0] S_AXI_AWADDR,
    input  wire        S_AXI_AWVALID,
    output wire        S_AXI_AWREADY,

    input  wire [31:0] S_AXI_WDATA,
    input  wire [3:0]  S_AXI_WSTRB,
    input  wire        S_AXI_WVALID,
    output wire        S_AXI_WREADY,

    output wire [1:0]  S_AXI_BRESP,
    output wire        S_AXI_BVALID,
    input  wire        S_AXI_BREADY,

    input  wire [31:0] S_AXI_ARADDR,
    input  wire        S_AXI_ARVALID,
    output wire        S_AXI_ARREADY,

    output wire [31:0] S_AXI_RDATA,
    output wire [1:0]  S_AXI_RRESP,
    output wire        S_AXI_RVALID,
    input  wire        S_AXI_RREADY,

    output wire [31:0] M_AXI_AWADDR,
    output wire [7:0]  M_AXI_AWLEN,
    output wire [2:0]  M_AXI_AWSIZE,
    output wire [1:0]  M_AXI_AWBURST,
    output wire        M_AXI_AWLOCK,
    output wire [3:0]  M_AXI_AWCACHE,
    output wire [2:0]  M_AXI_AWPROT,
    output wire        M_AXI_AWVALID,
    input  wire        M_AXI_AWREADY,

    output wire [31:0] M_AXI_WDATA,
    output wire [3:0]  M_AXI_WSTRB,
    output wire        M_AXI_WLAST,
    output wire        M_AXI_WVALID,
    input  wire        M_AXI_WREADY,

    input  wire [1:0]  M_AXI_BRESP,
    input  wire        M_AXI_BVALID,
    output wire        M_AXI_BREADY,

    output wire [31:0] M_AXI_ARADDR,
    output wire [7:0]  M_AXI_ARLEN,
    output wire [2:0]  M_AXI_ARSIZE,
    output wire [1:0]  M_AXI_ARBURST,
    output wire        M_AXI_ARLOCK,
    output wire [3:0]  M_AXI_ARCACHE,
    output wire [2:0]  M_AXI_ARPROT,
    output wire        M_AXI_ARVALID,
    input  wire        M_AXI_ARREADY,

    input  wire [31:0] M_AXI_RDATA,
    input  wire [1:0]  M_AXI_RRESP,
    input  wire        M_AXI_RLAST,
    input  wire        M_AXI_RVALID,
    output wire        M_AXI_RREADY
);

    gpu_soc_wrapper u_gpu_soc_wrapper_sv (
        .S_AXI_ACLK    (S_AXI_ACLK),
        .S_AXI_ARESETN (S_AXI_ARESETN),
        .S_AXI_AWADDR  (S_AXI_AWADDR),
        .S_AXI_AWVALID (S_AXI_AWVALID),
        .S_AXI_AWREADY (S_AXI_AWREADY),
        .S_AXI_WDATA   (S_AXI_WDATA),
        .S_AXI_WSTRB   (S_AXI_WSTRB),
        .S_AXI_WVALID  (S_AXI_WVALID),
        .S_AXI_WREADY  (S_AXI_WREADY),
        .S_AXI_BRESP   (S_AXI_BRESP),
        .S_AXI_BVALID  (S_AXI_BVALID),
        .S_AXI_BREADY  (S_AXI_BREADY),
        .S_AXI_ARADDR  (S_AXI_ARADDR),
        .S_AXI_ARVALID (S_AXI_ARVALID),
        .S_AXI_ARREADY (S_AXI_ARREADY),
        .S_AXI_RDATA   (S_AXI_RDATA),
        .S_AXI_RRESP   (S_AXI_RRESP),
        .S_AXI_RVALID  (S_AXI_RVALID),
        .S_AXI_RREADY  (S_AXI_RREADY),
        .M_AXI_AWADDR  (M_AXI_AWADDR),
        .M_AXI_AWLEN   (M_AXI_AWLEN),
        .M_AXI_AWSIZE  (M_AXI_AWSIZE),
        .M_AXI_AWBURST (M_AXI_AWBURST),
        .M_AXI_AWLOCK  (M_AXI_AWLOCK),
        .M_AXI_AWCACHE (M_AXI_AWCACHE),
        .M_AXI_AWPROT  (M_AXI_AWPROT),
        .M_AXI_AWVALID (M_AXI_AWVALID),
        .M_AXI_AWREADY (M_AXI_AWREADY),
        .M_AXI_WDATA   (M_AXI_WDATA),
        .M_AXI_WSTRB   (M_AXI_WSTRB),
        .M_AXI_WLAST   (M_AXI_WLAST),
        .M_AXI_WVALID  (M_AXI_WVALID),
        .M_AXI_WREADY  (M_AXI_WREADY),
        .M_AXI_BRESP   (M_AXI_BRESP),
        .M_AXI_BVALID  (M_AXI_BVALID),
        .M_AXI_BREADY  (M_AXI_BREADY),
        .M_AXI_ARADDR  (M_AXI_ARADDR),
        .M_AXI_ARLEN   (M_AXI_ARLEN),
        .M_AXI_ARSIZE  (M_AXI_ARSIZE),
        .M_AXI_ARBURST (M_AXI_ARBURST),
        .M_AXI_ARLOCK  (M_AXI_ARLOCK),
        .M_AXI_ARCACHE (M_AXI_ARCACHE),
        .M_AXI_ARPROT  (M_AXI_ARPROT),
        .M_AXI_ARVALID (M_AXI_ARVALID),
        .M_AXI_ARREADY (M_AXI_ARREADY),
        .M_AXI_RDATA   (M_AXI_RDATA),
        .M_AXI_RRESP   (M_AXI_RRESP),
        .M_AXI_RLAST   (M_AXI_RLAST),
        .M_AXI_RVALID  (M_AXI_RVALID),
        .M_AXI_RREADY  (M_AXI_RREADY)
    );

endmodule
