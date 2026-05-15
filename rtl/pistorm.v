/*
 * Copyright 2020 Claude Schwarz
 * Copyright 2020 Niklas Ekström - rewrite in Verilog
 */
module pistorm(
    output reg      PI_TXN_IN_PROGRESS, // GPIO0
    output reg      PI_IPL_ZERO,        // GPIO1
    input   [1:0]   PI_A,       // GPIO[3..2]
    input           PI_CLK,     // GPIO4
    output reg      PI_RESET,   // GPIO5
    input           PI_RD,      // GPIO6
    input           PI_WR,      // GPIO7
    inout   [15:0]  PI_D,       // GPIO[23..8]

    output reg      LTCH_A_0,
    output reg      LTCH_A_8,
    output reg      LTCH_A_16,
    output reg      LTCH_A_24,
    output reg      LTCH_A_OE_n,
    output reg      LTCH_D_RD_U,
    output reg      LTCH_D_RD_L,
    output reg      LTCH_D_RD_OE_n,
    output reg      LTCH_D_WR_U,
    output reg      LTCH_D_WR_L,
    output reg      LTCH_D_WR_OE_n,

    input           M68K_CLK,
    inout       [2:0] M68K_FC,

    inout           M68K_AS_n,
    inout           M68K_UDS_n,
    inout           M68K_LDS_n,
    inout           M68K_RW,

    input           M68K_DTACK_n,
    input           M68K_BERR_n,

    input           M68K_VPA_n,
    output reg      M68K_E,
    output reg      M68K_VMA_n,

    input   [2:0]   M68K_IPL_n,

    inout           M68K_RESET_n,
    inout           M68K_HALT_n,

    input           M68K_BR_n,
    output reg      M68K_BG_n,
    input           M68K_BGACK_n,
    input           M68K_C1,
    input           M68K_C3,
    input           CLK_SEL
  );

  wire c200m = PI_CLK;
  reg [2:0] c7m_sync;
//  wire c7m = M68K_CLK;
  wire c7m = c7m_sync[2];
  wire c1c3_clk = !(M68K_C1 ^ M68K_C3);

  localparam REG_DATA = 2'd0;
  localparam REG_ADDR_LO = 2'd1;
  localparam REG_ADDR_HI = 2'd2;
  localparam REG_STATUS = 2'd3;

  // Bus ownership: 1 = PiStorm drives the 68k bus, 0 = released to another master.
  reg       bus_owned = 1'b1;

  // Internal shadow regs for the tri-stateable CPU signals.
  reg [2:0] fc_r    = 3'd0;
  reg       as_n_r  = 1'b1;
  reg       uds_n_r = 1'b1;
  reg       lds_n_r = 1'b1;
  reg       rw_r    = 1'b1;

  assign M68K_FC    = bus_owned ? fc_r    : 3'bzzz;
  assign M68K_AS_n  = bus_owned ? as_n_r  : 1'bz;
  assign M68K_UDS_n = bus_owned ? uds_n_r : 1'bz;
  assign M68K_LDS_n = bus_owned ? lds_n_r : 1'bz;
  assign M68K_RW    = bus_owned ? rw_r    : 1'bz;

  initial begin
    PI_TXN_IN_PROGRESS <= 1'b0;
    PI_IPL_ZERO <= 1'b0;

    PI_RESET <= 1'b0;

    fc_r <= 3'd0;

    rw_r <= 1'b1;

    M68K_E <= 1'b0;
    M68K_VMA_n <= 1'b1;

    M68K_BG_n <= 1'b1;
  end

  reg [1:0] rd_sync;
  reg [1:0] wr_sync;

  always @(posedge c200m) begin
    rd_sync <= {rd_sync[0], PI_RD};
    wr_sync <= {wr_sync[0], PI_WR};
  end

  wire rd_rising = !rd_sync[1] && rd_sync[0];
  wire wr_rising = !wr_sync[1] && wr_sync[0];

  reg [15:0] data_out;
  reg [2:0]  ipl;  // forward-declared so iverilog accepts the use below; full def further down
  assign PI_D = PI_A == REG_STATUS && PI_RD ? data_out : 16'bz;

  always @(posedge c200m) begin
    if (rd_rising && PI_A == REG_STATUS) begin
      data_out <= {ipl, 13'd0};
    end
  end

  reg [15:0] status;
  wire reset_out = !status[1];

  assign M68K_RESET_n = reset_out ? 1'b0 : 1'bz;
  assign M68K_HALT_n = reset_out ? 1'b0 : 1'bz;

  reg op_req = 1'b0;
  reg op_rw = 1'b1;
  reg op_uds_n = 1'b1;
  reg op_lds_n = 1'b1;

  always @(*) begin
    LTCH_D_WR_U <= PI_A == REG_DATA && PI_WR;
    LTCH_D_WR_L <= PI_A == REG_DATA && PI_WR;

    LTCH_A_0 <= PI_A == REG_ADDR_LO && PI_WR;
    LTCH_A_8 <= PI_A == REG_ADDR_LO && PI_WR;

    LTCH_A_16 <= PI_A == REG_ADDR_HI && PI_WR;
    LTCH_A_24 <= PI_A == REG_ADDR_HI && PI_WR;

    LTCH_D_RD_OE_n <= !(PI_A == REG_DATA && PI_RD);
  end

  reg a0;

  always @(posedge c200m) begin
    c7m_sync <= {c7m_sync[1:0], (CLK_SEL?M68K_CLK:c1c3_clk)};
  end

  wire c7m_rising = !c7m_sync[2] && c7m_sync[1];
  wire c7m_falling = c7m_sync[2] && !c7m_sync[1];

  reg [2:0] ipl_1;
  reg [2:0] ipl_2;

  always @(posedge c200m) begin
    if (c7m_falling) begin
      ipl_1 <= ~M68K_IPL_n;
      ipl_2 <= ipl_1;
    end

    if (ipl_2 == ipl_1)
      ipl <= ipl_2;

    PI_IPL_ZERO <= ipl == 3'd0;
  end

  always @(posedge c200m) begin
    PI_RESET <= reset_out ? 1'b1 : M68K_RESET_n;
  end

  reg [3:0] e_counter = 4'd0;

  always @(negedge c7m) begin
    if (e_counter == 4'd9)
      e_counter <= 4'd0;
    else
      e_counter <= e_counter + 4'd1;
  end

  always @(negedge c7m) begin
    if (e_counter == 4'd9)
      M68K_E <= 1'b0;
    else if (e_counter == 4'd5)
      M68K_E <= 1'b1;
  end

  reg [2:0] state = 3'd0;
  reg [2:0] PI_TXN_IN_PROGRESS_delay;

  // -------- Bus arbitration (BR/BG/BGACK) --------
  // Two-flop synchronizers for async inputs from the other master.
  reg [1:0] br_sync    = 2'b11;
  reg [1:0] bgack_sync = 2'b11;
  always @(posedge c200m) begin
    br_sync    <= {br_sync[0],    M68K_BR_n};
    bgack_sync <= {bgack_sync[0], M68K_BGACK_n};
  end
  wire br_n_s    = br_sync[1];
  wire bgack_n_s = bgack_sync[1];

  // We only release the bus at a clean boundary: parked in the wait-for-op
  // state with AS deasserted and no queued op. S0 only lasts one PI_CLK and
  // unconditionally falls into S1, where the FSM idles until op_req goes high
  // — so S1 (or transiently S0) is the right place to grant. LTCH_*_OE_n are
  // already raised by S7 before we get here.
  wire bus_idle = (state == 3'd0 || state == 3'd1) && as_n_r && !op_req;

  localparam ARB_IDLE     = 2'd0;
  localparam ARB_GRANTING = 2'd1;  // BG asserted, waiting for BGACK
  localparam ARB_RELEASED = 2'd2;  // BGACK asserted, off the bus
  reg [1:0] arb_state = ARB_IDLE;

  always @(posedge c200m) begin
    case (arb_state)
      ARB_IDLE: begin
        if (!br_n_s && bus_idle) begin
          M68K_BG_n <= 1'b0;       // grant
          arb_state <= ARB_GRANTING;
        end
      end
      ARB_GRANTING: begin
        if (!bgack_n_s) begin
          bus_owned <= 1'b0;       // tri-state CPU-driven signals
          M68K_BG_n <= 1'b1;       // BG can be released once BGACK is asserted
          arb_state <= ARB_RELEASED;
        end
      end
      ARB_RELEASED: begin
        if (bgack_n_s) begin       // other master finished
          bus_owned <= 1'b1;
          arb_state <= ARB_IDLE;
        end
      end
      default: arb_state <= ARB_IDLE;
    endcase
  end

  always @(posedge c200m) begin

    // Always accept Pi register writes — the Pi can queue an op while
    // we're released; it will execute once the bus comes back.
    if (wr_rising) begin
      case (PI_A)
        REG_ADDR_LO: begin
          a0 <= PI_D[0];
          PI_TXN_IN_PROGRESS <= 1'b1;
        end
        REG_ADDR_HI: begin
          op_req <= 1'b1;
          op_rw <= PI_D[9];
          op_uds_n <= PI_D[8] ? a0 : 1'b0;
          op_lds_n <= PI_D[8] ? !a0 : 1'b0;
        end
        REG_STATUS: begin
          status <= PI_D;
        end
      endcase
    end

    // 68k cycle FSM. We only block the S1->S2 advance when we don't own the
    // bus (gating the whole FSM here added ~1.6 ns to every next-state path).
    // The arbiter is guaranteed to release only at S0, so the FSM will be
    // parked at S0 or S1 (the wait-for-op state) when bus_owned drops.
    case (state)
      3'd0: begin // S0
        rw_r <= 1'b1; // S7 -> S0
//        if (c7m_falling) begin
//          if (op_req) begin
            state <= 2'd1;
//          end
//        end
      end

      3'd1: begin // S1
        // Only start a new 68k cycle when the arbiter is idle. This is
        // strictly tighter than gating on bus_owned alone: it also blocks
        // new cycles while BG is asserted and we're waiting for BGACK
        // (the ARB_GRANTING window), which a real 68k won't do either.
        // ARB_IDLE implies bus_owned == 1, so the bus_owned term is folded in.
        if (op_req && arb_state == ARB_IDLE) begin
          if(c7m_rising) begin
            state <= 3'd2;
          end
        end
      end
      3'd2: begin // S2
        rw_r <= op_rw; // S1 -> S2
        LTCH_D_WR_OE_n <= op_rw;
        LTCH_A_OE_n <= 1'b0;
        as_n_r <= 1'b0;
        uds_n_r <= op_rw ? op_uds_n : 1'b1;
        lds_n_r <= op_rw ? op_lds_n : 1'b1;
        if (c7m_falling) begin
          uds_n_r <= op_uds_n;
          lds_n_r <= op_lds_n;
          state <= 3'd3;
        end
      end

      3'd3: begin // S3
        op_req <= 1'b0;
        if(c7m_rising) begin
          if (!M68K_DTACK_n || (!M68K_VMA_n && e_counter == 4'd8)) begin
            state <= 3'd4;
            PI_TXN_IN_PROGRESS_delay[2:0] <= 3'b111;
          end
          else begin
            if (!M68K_VPA_n && e_counter == 4'd2) begin
              M68K_VMA_n <= 1'b0;
            end
          end
        end
      end
      3'd4: begin // S4
        PI_TXN_IN_PROGRESS_delay <= {PI_TXN_IN_PROGRESS_delay[1:0],1'b0};
        PI_TXN_IN_PROGRESS <= PI_TXN_IN_PROGRESS_delay[2];
        LTCH_D_RD_U <= 1'b1;
        LTCH_D_RD_L <= 1'b1;
        if (c7m_falling) begin
          state <= 3'd5;
          PI_TXN_IN_PROGRESS <= 1'b0;
        end
      end

      3'd5: begin // S5
        LTCH_D_RD_U <= 1'b0;
        LTCH_D_RD_L <= 1'b0;
        if (c7m_rising) begin
          state <= 3'd6;
        end
      end
       
      3'd6: begin // S6
        if (c7m_falling) begin
          M68K_VMA_n <= 1'b1;
          state <= 3'd7;
        end
      end
       
      3'd7: begin // S7
        LTCH_D_WR_OE_n <= 1'b1;
        LTCH_A_OE_n <= 1'b1;
        as_n_r <= 1'b1;
        uds_n_r <= 1'b1;
        lds_n_r <= 1'b1;
//        if(c7m_rising) begin
//          rw_r <= 1'b1; // S7 -> S0
          state <= 3'd0;
//        end
      end
    endcase
  end

endmodule
