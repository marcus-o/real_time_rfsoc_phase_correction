open_project -reset proj
open_solution "solution1" -flow_target vivado

set_top passer_32_double
add_files passer_32_double.cpp 
add_files passer_32_double.h 
#add_files -tb passer_32_double.cpp 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.1 -name default

config_export -format ip_catalog -rtl verilog

csynth_design
export_design

exit