open_project -reset proj
open_solution "solution1" -flow_target vivado

set_top axi_dump
add_files axi_dump.cpp 
add_files axi_dump.h 
# add_files -tb dut_tb.cpp 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.17 -name default

config_export -format ip_catalog -rtl verilog

csynth_design
export_design

exit