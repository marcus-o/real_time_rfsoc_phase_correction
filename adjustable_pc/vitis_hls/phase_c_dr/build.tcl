open_project -reset dma_passer
open_solution "dma_passer" -flow_target vivado

set_top dma_passer
add_files dma_passer.cpp 
add_files pc_dr.h 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.1 -name default

config_export -format ip_catalog -rtl verilog

csynth_design
export_design


open_project -reset dma_writer
open_solution "dma_writer" -flow_target vivado

set_top dma_writer
add_files dma_writer.cpp 
add_files pc_dr.h 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.1 -name default

config_export -format ip_catalog -rtl verilog

csynth_design
export_design


open_project -reset hilbert_transform
open_solution "hilbert_transform" -flow_target vivado

set_top hilbert_transform
add_files hilbert_transform.cpp 
add_files pc_dr.h 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.1 -name default

config_export -format ip_catalog -rtl verilog

csynth_design
export_design


open_project -reset measure_worker
open_solution "measure_worker" -flow_target vivado

set_top measure_worker
add_files measure_worker.cpp 
add_files pc_dr.h 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.1 -name default

config_export -format ip_catalog -rtl verilog

csynth_design
export_design


open_project -reset pc_averager
open_solution "pc_averager" -flow_target vivado

set_top pc_averager
add_files pc_averager.cpp 
add_files pc_dr.h 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.1 -name default

config_export -format ip_catalog -rtl verilog

csynth_design
export_design


open_project -reset pc_dr
open_solution "pc_dr" -flow_target vivado

set_top pc_dr
add_files pc_dr.cpp 
add_files pc_dr.h 
add_files -tb pc_dr_test.cpp 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.1 -name default

config_export -format ip_catalog -rtl verilog

csynth_design
export_design

exit