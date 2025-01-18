open_project -reset proj
open_solution "solution1" -flow_target vivado

set_top pc_dr
add_files pc_dr.cpp 
add_files pc_dr.h 
add_files -tb pc_dr_test.cpp 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.17 -name default

config_export -format ip_catalog -rtl verilog

csynth_design
export_design

exit