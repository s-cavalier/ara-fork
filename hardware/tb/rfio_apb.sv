// Copyright 2026
//
// Socket-backed RF I/O APB peripheral for Ara Verilator simulations.
//
// Address map, relative to Ara's UART peripheral base at 0xC000_0000:
//   0x000: LOG_TX       write low byte to simulator stdout
//   0x100: RFIO_RX_DATA read one little-endian u32 from host socket
//   0x104: RFIO_TX_DATA write one little-endian u32 to host socket
//   0x108: RFIO_STATUS  bit 0 RX_VALID, bit 1 TX_READY, bit 2 CONNECTED

module rfio_apb (
    input  logic        clk_i,
    input  logic        rst_ni,
    input  logic        penable_i,
    input  logic        pwrite_i,
    input  logic [31:0] paddr_i,
    input  logic        psel_i,
    input  logic [31:0] pwdata_i,
    output logic [31:0] prdata_o,
    output logic        pready_o,
    output logic        pslverr_o
);

  import "DPI-C" context function int unsigned rfio_dpi_status();
  import "DPI-C" context function int unsigned rfio_dpi_recv_u32();
  import "DPI-C" context function void rfio_dpi_send_u32(input int unsigned value);
  import "DPI-C" context function void rfio_dpi_log_byte(input int unsigned value);

  localparam logic [11:0] LOG_TX       = 12'h000;
  localparam logic [11:0] RFIO_RX_DATA = 12'h100;
  localparam logic [11:0] RFIO_TX_DATA = 12'h104;
  localparam logic [11:0] RFIO_STATUS  = 12'h108;

  logic [31:0] prdata_q;

  // The APB bridge presents a one-cycle setup phase followed by an access
  // phase. Execute side effects in setup so read data is stable for access.
  wire setup_phase = psel_i && !penable_i;
  wire [11:0] addr = paddr_i[11:0];

  always_ff @(posedge clk_i or negedge rst_ni) begin
    if (!rst_ni) begin
      prdata_q <= '0;
    end else if (setup_phase) begin
      if (pwrite_i) begin
        unique case (addr)
          LOG_TX:       rfio_dpi_log_byte(pwdata_i);
          RFIO_TX_DATA: rfio_dpi_send_u32(pwdata_i);
          default: ;
        endcase
      end else begin
        unique case (addr)
          RFIO_RX_DATA: prdata_q <= rfio_dpi_recv_u32();
          RFIO_STATUS:  prdata_q <= rfio_dpi_status();
          default:      prdata_q <= '0;
        endcase
      end
    end
  end

  assign prdata_o  = prdata_q;
  assign pready_o  = 1'b1;
  assign pslverr_o = 1'b0;

endmodule : rfio_apb
