## Generated SDC file "pistorm.sdc"

# Time Information
set_time_format -unit ns -decimal_places 3

# -----------------------------------------------------------------------------
# Clocks
# -----------------------------------------------------------------------------
# External base clocks
create_clock -name {PI_CLK}   -period 5.000   [get_ports {PI_CLK}]
create_clock -name {M68K_CLK} -period 141.000 [get_ports {M68K_CLK}]
create_clock -name {M68K_C1}  -period 282.000 [get_ports {M68K_C1}]
create_clock -name {M68K_C3}  -period 282.000 [get_ports {M68K_C3}]

# -----------------------------------------------------------------------------
# Clock Domains
# -----------------------------------------------------------------------------
# Isolate the fast Pi clock from the slow Amiga clocks
set_clock_groups -asynchronous \
    -group [get_clocks {PI_CLK}] \
    -group [get_clocks {M68K_CLK M68K_C1 M68K_C3}]

# -----------------------------------------------------------------------------
# False Paths (I/O Constraints)
# -----------------------------------------------------------------------------
# Static async configuration pin
set_false_path -from [get_ports {CLK_SEL}]

# Asynchronous inputs
set_false_path -from [get_ports { \
    M68K_CLK \
    M68K_DTACK_n \
    M68K_VPA_n \
    M68K_IPL_n[*] \
    M68K_BR_n \
    M68K_BGACK_n \
    M68K_RESET_n \
    M68K_HALT_n \
    PI_A[*] \
    PI_D[*] \
    PI_RD \
    PI_WR \
}]

# Asynchronous outputs
set_false_path -to [get_ports { \
    LTCH_A_0 \
    LTCH_A_8 \
    LTCH_A_16 \
    LTCH_A_24 \
    LTCH_A_OE_n \
    LTCH_D_RD_L \
    LTCH_D_RD_OE_n \
    LTCH_D_RD_U \
    LTCH_D_WR_L \
    LTCH_D_WR_OE_n \
    LTCH_D_WR_U \
    M68K_AS_n \
    M68K_BG_n \
    M68K_E \
    M68K_FC[*] \
    M68K_HALT_n \
    M68K_LDS_n \
    M68K_RESET_n \
    M68K_RW \
    M68K_UDS_n \
    M68K_VMA_n \
    PI_TXN_IN_PROGRESS \
    PI_IPL_ZERO \
    PI_D[*] \
    PI_RESET \
}]
