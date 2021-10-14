## Generated SDC file "pistorm.sdc"

## Copyright (C) 2020  Intel Corporation. All rights reserved.
## Your use of Intel Corporation's design tools, logic functions 
## and other software and tools, and any partner logic 
## functions, and any output files from any of the foregoing 
## (including device programming or simulation files), and any 
## associated documentation or information are expressly subject 
## to the terms and conditions of the Intel Program License 
## Subscription Agreement, the Intel Quartus Prime License Agreement,
## the Intel FPGA IP License Agreement, or other applicable license
## agreement, including, without limitation, that your use is for
## the sole purpose of programming logic devices manufactured by
## Intel and sold by Intel or its authorized distributors.  Please
## refer to the applicable agreement for further details, at
## https://fpgasoftware.intel.com/eula.


## VENDOR  "Altera"
## PROGRAM "Quartus Prime"
## VERSION "Version 20.1.1 Build 720 11/11/2020 SJ Lite Edition"

## DATE    "Sun Dec 20 15:18:48 2020"

##
## DEVICE  "EPM570T100C5"
##


#**************************************************************
# Time Information
#**************************************************************

set_time_format -unit ns -decimal_places 3



#**************************************************************
# Create Clock
#**************************************************************

create_clock -name {PI_CLK} -period 5.000 [get_ports {PI_CLK}]
create_clock -name {M68K_CLK} -period 141.000 [get_ports {M68K_CLK}]
create_clock -name {M68K_C1} -period 282.000 [get_ports {M68K_C1}]
create_clock -name {M68K_C3} -period 282.000 [get_ports {M68K_C3}]

#**************************************************************
# Create Generated Clock
#**************************************************************



#**************************************************************
# Set Clock Latency
#**************************************************************



#**************************************************************
# Set Clock Uncertainty
#**************************************************************



#**************************************************************
# Set Input Delay
#**************************************************************



#**************************************************************
# Set Output Delay
#**************************************************************



#**************************************************************
# Set Clock Groups
#**************************************************************



#**************************************************************
# Set False Path
#**************************************************************

set_false_path -from [get_ports {M68K_CLK M68K_DTACK_n M68K_VPA_n M68K_IPL_n[*] PI_A[*] PI_D[*] PI_RD PI_WR}]
set_false_path -to [get_ports {LTCH_A_0 LTCH_A_8 LTCH_A_16 LTCH_A_24 LTCH_A_OE_n LTCH_D_RD_L LTCH_D_RD_OE_n LTCH_D_RD_U LTCH_D_WR_L LTCH_D_WR_OE_n LTCH_D_WR_U M68K_AS_n M68K_BG_n M68K_E M68K_FC[*] M68K_HALT_n M68K_LDS_n M68K_RESET_n M68K_RW M68K_UDS_n M68K_VMA_n PI_TXN_IN_PROGRESS PI_IPL_ZERO PI_D[*]}]

set_false_path -from [get_clocks {M68K_CLK}] -to [get_clocks {PI_CLK}]
set_false_path -from [get_clocks {PI_CLK}] -to [get_clocks {M68K_CLK}]

#**************************************************************
# Set Multicycle Path
#**************************************************************



#**************************************************************
# Set Maximum Delay
#**************************************************************



#**************************************************************
# Set Minimum Delay
#**************************************************************



#**************************************************************
# Set Input Transition
#**************************************************************

