// Testbench for pistorm.v — focuses on the BR/BG/BGACK arbitration logic
// added on top of the existing CPU-cycle FSM.
//
// Run:
//   iverilog -g2012 -o pistorm_tb pistorm_tb.v pistorm.v
//   vvp pistorm_tb            # text log to stdout
//   gtkwave pistorm_tb.vcd    # waveform viewer

`timescale 1ns/100ps

module pistorm_tb;

  // --- Clocks ---
  reg PI_CLK = 0;            // 200 MHz
  reg M68K_CLK = 0;          //   7.09 MHz (141 ns period per SDC)
  always #2.5  PI_CLK   = ~PI_CLK;
  always #70.5 M68K_CLK = ~M68K_CLK;

  // --- DUT I/O ---
  reg  [1:0]  PI_A = 0;
  reg         PI_RD = 0;
  reg         PI_WR = 0;
  wire [15:0] PI_D;             // inout
  reg  [15:0] pi_d_drv = 16'bz; // host-side driver
  reg         pi_d_drv_en = 0;
  assign PI_D = pi_d_drv_en ? pi_d_drv : 16'bz;

  wire        PI_TXN_IN_PROGRESS, PI_IPL_ZERO, PI_RESET;

  // The 68k bus signals are mostly bidirectional; model the wired-OR / open
  // collector with pullups so any side can pull them low.
  wire [2:0]  M68K_FC;
  wire        M68K_AS_n;
  wire        M68K_UDS_n;
  wire        M68K_LDS_n;
  wire        M68K_RW;
  pullup(M68K_AS_n);
  pullup(M68K_UDS_n);
  pullup(M68K_LDS_n);
  pullup(M68K_RW);

  // DTACK / VPA / BERR / IPL: driven by the testbench standing in for the
  // Amiga motherboard.
  reg         dtack_drv_n = 1'bz; assign (pull0,pull1) M68K_DTACK_n_w = dtack_drv_n;
  wire        M68K_DTACK_n; assign M68K_DTACK_n = dtack_drv_n;
  reg         M68K_VPA_n = 1'b1;
  reg         M68K_BERR_n = 1'b1;
  reg  [2:0]  M68K_IPL_n = 3'b111;
  wire        M68K_E, M68K_VMA_n;

  // RESET / HALT are open-drain on the Amiga; let the DUT and the TB share.
  wire        M68K_RESET_n; pullup(M68K_RESET_n);
  wire        M68K_HALT_n;  pullup(M68K_HALT_n);

  // BR / BGACK are inputs to the DUT (we drive them as a fake bus master);
  // BG is an output from the DUT.
  reg         M68K_BR_n    = 1'b1;
  wire        M68K_BG_n;
  reg         M68K_BGACK_n = 1'b1;

  // C1/C3/CLK_SEL — pick simple M68K_CLK passthrough.
  reg         M68K_C1 = 0, M68K_C3 = 0;
  reg         CLK_SEL = 1'b1;  // use M68K_CLK directly

  // --- Latch / address-bus model (just enough to satisfy the DUT) ---
  wire LTCH_A_0, LTCH_A_8, LTCH_A_16, LTCH_A_24, LTCH_A_OE_n;
  wire LTCH_D_RD_U, LTCH_D_RD_L, LTCH_D_RD_OE_n;
  wire LTCH_D_WR_U, LTCH_D_WR_L, LTCH_D_WR_OE_n;

  // --- DUT ---
  pistorm dut(
    .PI_TXN_IN_PROGRESS(PI_TXN_IN_PROGRESS),
    .PI_IPL_ZERO(PI_IPL_ZERO),
    .PI_A(PI_A), .PI_CLK(PI_CLK), .PI_RESET(PI_RESET),
    .PI_RD(PI_RD), .PI_WR(PI_WR), .PI_D(PI_D),
    .LTCH_A_0(LTCH_A_0), .LTCH_A_8(LTCH_A_8),
    .LTCH_A_16(LTCH_A_16), .LTCH_A_24(LTCH_A_24),
    .LTCH_A_OE_n(LTCH_A_OE_n),
    .LTCH_D_RD_U(LTCH_D_RD_U), .LTCH_D_RD_L(LTCH_D_RD_L),
    .LTCH_D_RD_OE_n(LTCH_D_RD_OE_n),
    .LTCH_D_WR_U(LTCH_D_WR_U), .LTCH_D_WR_L(LTCH_D_WR_L),
    .LTCH_D_WR_OE_n(LTCH_D_WR_OE_n),
    .M68K_CLK(M68K_CLK), .M68K_FC(M68K_FC),
    .M68K_AS_n(M68K_AS_n), .M68K_UDS_n(M68K_UDS_n),
    .M68K_LDS_n(M68K_LDS_n), .M68K_RW(M68K_RW),
    .M68K_DTACK_n(M68K_DTACK_n), .M68K_BERR_n(M68K_BERR_n),
    .M68K_VPA_n(M68K_VPA_n), .M68K_E(M68K_E), .M68K_VMA_n(M68K_VMA_n),
    .M68K_IPL_n(M68K_IPL_n),
    .M68K_RESET_n(M68K_RESET_n), .M68K_HALT_n(M68K_HALT_n),
    .M68K_BR_n(M68K_BR_n), .M68K_BG_n(M68K_BG_n), .M68K_BGACK_n(M68K_BGACK_n),
    .M68K_C1(M68K_C1), .M68K_C3(M68K_C3), .CLK_SEL(CLK_SEL)
  );

  // --- Fake slave: drop DTACK when AS asserts ---
  always @(negedge M68K_AS_n) begin
    repeat (3) @(posedge M68K_CLK);   // 3 clk delay, like real RAM
    dtack_drv_n <= 1'b0;
  end
  always @(posedge M68K_AS_n) dtack_drv_n <= 1'bz;

  // --- Helpers ---
  task pi_write(input [1:0] a, input [15:0] d);
    begin
      @(posedge PI_CLK); #1;
      PI_A <= a; pi_d_drv <= d; pi_d_drv_en <= 1'b1;
      PI_WR <= 1'b1;
      @(posedge PI_CLK); #1;
      PI_WR <= 1'b0;
      @(posedge PI_CLK); #1;
      pi_d_drv_en <= 1'b0;
    end
  endtask

  // Issue a 68k read of address `addr`. ADDR_LO carries low 16 bits + a0,
  // ADDR_HI carries high 8 bits + control bits (bit9 = read, bit8 = byte).
  task pi_cpu_read_word(input [23:0] addr);
    begin
      pi_write(2'd1, {addr[15:0]});
      pi_write(2'd2, {6'b0, 1'b1 /*read*/, 1'b0 /*word*/, addr[23:16]});
    end
  endtask

  // --- Protocol checks ---
  reg fail = 0;
  task fail_if(input cond, input [255:0] msg);
    begin
      if (cond) begin
        $display("[FAIL @ %0t] %0s", $time, msg);
        fail = 1;
      end
    end
  endtask

  // Whenever we don't own the bus, the DUT must NOT drive AS/UDS/LDS/RW.
  // (Sampled on PI_CLK to filter sim-time glitches around tri-state handoff.)
  always @(posedge PI_CLK) begin
    if (dut.bus_owned == 1'b0) begin
      fail_if(M68K_AS_n  !== 1'b1, "AS_n driven low while bus released");
      fail_if(M68K_UDS_n !== 1'b1, "UDS_n driven low while bus released");
      fail_if(M68K_LDS_n !== 1'b1, "LDS_n driven low while bus released");
    end
  end

  // BG must only assert at a clean idle point (FSM in S0/S1 wait, AS high).
  always @(negedge M68K_BG_n) begin
    fail_if(!(dut.state == 3'd0 || dut.state == 3'd1),
            "BG asserted while FSM not in S0/S1 wait");
    fail_if(M68K_AS_n  !== 1'b1, "BG asserted while AS still low");
  end

  // --- Main stimulus ---
  initial begin
    $dumpfile("pistorm_tb.vcd");
    $dumpvars(0, pistorm_tb);

    // Release reset
    pi_write(2'd3, 16'h0002); // REG_STATUS bit1=1 -> reset_out=0
    #200;

    // ----- Scenario 1: normal CPU read, no BR -----
    $display("[%0t] Scenario 1: normal CPU read", $time);
    pi_cpu_read_word(24'h00_1234);
    // Wait for the cycle to complete
    wait (PI_TXN_IN_PROGRESS == 1'b1);
    wait (PI_TXN_IN_PROGRESS == 1'b0);
    $display("[%0t]   cycle completed (state=%0d)", $time, dut.state);

    #200;

    // ----- Scenario 2: external master requests bus -----
    $display("[%0t] Scenario 2: BR while idle", $time);
    M68K_BR_n <= 1'b0;
    wait (M68K_BG_n == 1'b0);
    $display("[%0t]   BG asserted (state=%0d, arb=%0d)",
             $time, dut.state, dut.arb_state);

    // Pretend to be the requester: pull BGACK low
    @(posedge PI_CLK); #1;
    M68K_BGACK_n <= 1'b0;
    M68K_BR_n    <= 1'b1;  // requester drops BR once BGACK is in

    wait (dut.bus_owned == 1'b0);
    $display("[%0t]   bus released (BG_n=%b, bus_owned=%b)",
             $time, M68K_BG_n, dut.bus_owned);
    fail_if(M68K_BG_n !== 1'b1, "BG should be deasserted after BGACK");

    // ----- Scenario 3: queue an op while we don't own the bus -----
    $display("[%0t] Scenario 3: queue a CPU op while released", $time);
    pi_cpu_read_word(24'h00_5678);
    #500;
    fail_if(dut.state !== 3'd1 && dut.state !== 3'd0,
            "FSM advanced past S1 while bus released");
    $display("[%0t]   FSM parked at state=%0d as expected", $time, dut.state);

    // Release the bus back to the DUT
    @(posedge PI_CLK); #1;
    M68K_BGACK_n <= 1'b1;

    wait (dut.bus_owned == 1'b1);
    $display("[%0t]   bus reclaimed", $time);

    // Queued op should now run
    wait (M68K_AS_n == 1'b0);
    $display("[%0t]   queued cycle started (AS low)", $time);
    wait (PI_TXN_IN_PROGRESS == 1'b0);
    $display("[%0t]   queued cycle completed", $time);

    #400;

    // ----- Scenario 4: BR asserted mid-CPU-cycle -----
    // Start a CPU read, then assert BR while AS is still low. Arbiter must
    // not grant until the cycle finishes (AS goes high again).
    $display("[%0t] Scenario 4: BR asserted mid-cycle", $time);
    pi_cpu_read_word(24'h00_9abc);
    wait (M68K_AS_n == 1'b0);
    $display("[%0t]   AS asserted, now raising BR", $time);
    M68K_BR_n <= 1'b0;
    // BG must NOT assert while AS is low. The assertion at line 145 catches
    // this continuously; we just need to wait and check it didn't fire.
    wait (M68K_AS_n == 1'b1);
    $display("[%0t]   cycle done (state=%0d), expecting BG soon", $time, dut.state);
    wait (M68K_BG_n == 1'b0);
    $display("[%0t]   BG granted (state=%0d)", $time, dut.state);

    @(posedge PI_CLK); #1;
    M68K_BGACK_n <= 1'b0;
    M68K_BR_n    <= 1'b1;
    wait (dut.bus_owned == 1'b0);
    @(posedge PI_CLK); #1;
    M68K_BGACK_n <= 1'b1;
    wait (dut.bus_owned == 1'b1);
    $display("[%0t]   released and reclaimed", $time);

    #400;

    // ----- Scenario 5: BR pulsed briefly (request retracted) -----
    // If BR drops back high before BG is granted, the arbiter shouldn't grant.
    // It might still grant if BR was sampled low through the synchronizer —
    // that's spec-compliant (grant honors any seen request), so this is a
    // soft check: we just observe what happens.
    $display("[%0t] Scenario 5: BR pulsed briefly", $time);
    M68K_BR_n <= 1'b0;
    repeat (1) @(posedge PI_CLK);    // 1 PI_CLK only — likely not through the 2-flop sync
    M68K_BR_n <= 1'b1;
    repeat (50) @(posedge PI_CLK);
    if (M68K_BG_n == 1'b0) begin
      $display("[%0t]   BG asserted from brief pulse (spec-compliant); completing handshake", $time);
      @(posedge PI_CLK); #1;
      M68K_BGACK_n <= 1'b0;
      wait (dut.bus_owned == 1'b0);
      @(posedge PI_CLK); #1;
      M68K_BGACK_n <= 1'b1;
      wait (dut.bus_owned == 1'b1);
    end else begin
      $display("[%0t]   pulse was filtered (BG stayed high) — OK", $time);
    end

    #400;

    // ----- Scenario 6: BG granted but BGACK never comes -----
    // If the bus master crashes/never asserts BGACK, the arbiter should stay
    // in ARB_GRANTING with BG asserted but not yet release the bus. Verify
    // that and then have the master finally ack to recover.
    $display("[%0t] Scenario 6: BG granted, BGACK delayed", $time);
    M68K_BR_n <= 1'b0;
    wait (M68K_BG_n == 1'b0);
    $display("[%0t]   BG asserted, withholding BGACK for ~1us", $time);
    repeat (200) @(posedge PI_CLK);
    fail_if(dut.bus_owned !== 1'b1, "bus_owned dropped without BGACK");
    fail_if(dut.arb_state !== 2'd1 /*ARB_GRANTING*/, "arb_state left ARB_GRANTING without BGACK");
    $display("[%0t]   arb still in GRANTING (arb=%0d, bus_owned=%b), now sending BGACK",
             $time, dut.arb_state, dut.bus_owned);

    @(posedge PI_CLK); #1;
    M68K_BGACK_n <= 1'b0;
    M68K_BR_n    <= 1'b1;
    wait (dut.bus_owned == 1'b0);
    @(posedge PI_CLK); #1;
    M68K_BGACK_n <= 1'b1;
    wait (dut.bus_owned == 1'b1);
    $display("[%0t]   recovered after delayed BGACK", $time);

    #400;

    // ----- Scenario 7: two grants back-to-back -----
    $display("[%0t] Scenario 7: two grants back-to-back", $time);
    repeat (2) begin : two_grants
      integer i;
      M68K_BR_n <= 1'b0;
      wait (M68K_BG_n == 1'b0);
      @(posedge PI_CLK); #1;
      M68K_BGACK_n <= 1'b0;
      M68K_BR_n    <= 1'b1;
      wait (dut.bus_owned == 1'b0);
      @(posedge PI_CLK); #1;
      M68K_BGACK_n <= 1'b1;
      wait (dut.bus_owned == 1'b1);
      for (i = 0; i < 20; i = i + 1) @(posedge PI_CLK);
    end
    $display("[%0t]   two grants completed cleanly", $time);

    #400;

    // ----- Scenario 8: BR asserted just before a queued op would advance ---
    // Pi queues an op; in the same window, BR is asserted. The arbiter must
    // wait for the in-flight cycle to finish before granting (S1 → S2 with
    // bus_owned still high is allowed if BR is just freshly synchronized).
    $display("[%0t] Scenario 8: BR coincident with queued op", $time);
    fork
      begin
        pi_cpu_read_word(24'h00_dead);
      end
      begin
        @(posedge PI_CLK); #1;
        M68K_BR_n <= 1'b0;
      end
    join

    // Let the system settle a bit so we can see who won the race.
    repeat (20) @(posedge PI_CLK);

    // Two valid orderings:
    //   (a) BR won — BG is asserted, FSM stalled in S1; we run the
    //       arbitration handshake first, then the queued op completes after.
    //   (b) The op started before BR was synchronized — cycle is in flight.
    if (M68K_BG_n == 1'b0) begin
      $display("[%0t]   BR won race; completing handshake first", $time);
      @(posedge PI_CLK); #1;
      M68K_BGACK_n <= 1'b0;
      M68K_BR_n    <= 1'b1;
      wait (dut.bus_owned == 1'b0);
      @(posedge PI_CLK); #1;
      M68K_BGACK_n <= 1'b1;
      wait (dut.bus_owned == 1'b1);
    end else begin
      $display("[%0t]   op won race; cycle already in flight", $time);
      M68K_BR_n <= 1'b1;
    end

    wait (M68K_AS_n == 1'b0);          // queued op finally runs
    wait (PI_TXN_IN_PROGRESS == 1'b0); // and completes
    $display("[%0t]   queued op completed", $time);

    #400;
    if (fail) $display("==== FAILED ====");
    else      $display("==== PASSED ====");
    $finish;
  end

  initial begin
    #500000 $display("[%0t] TIMEOUT", $time);
    $fatal;
  end

endmodule
