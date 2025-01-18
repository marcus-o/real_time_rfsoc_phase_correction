open_project -reset proj
open_solution "solution1" -flow_target vivado

set_top const_config_writer
add_files const_config_writer.cpp 
add_files const_config_writer.h 
# add_files -tb dut_tb.cpp 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.17 -name default

config_export -format ip_catalog -rtl verilog

csynth_design
export_design

exit