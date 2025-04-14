open_project -reset proj
open_solution "solution1" -flow_target vivado

set_top saturate_stream_dr
add_files saturate_stream_dr.cpp 
add_files saturate_stream_dr.h 
add_files -tb saturate_stream_dr_test.cpp 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.17 -name default

config_export -format ip_catalog -rtl verilog

csynth_design
export_design

exit