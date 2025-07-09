
open_project -reset c12_to_16
open_solution "c12_to_16" -flow_target vivado
set_top c12_to_16
add_files c12_to_16.cpp 
add_files pc_dr.h 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 2.3 -name default
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

open_project -reset trigger_worker
open_solution "trigger_worker" -flow_target vivado
set_top trigger_worker
add_files trigger_worker.cpp 
add_files pc_dr.h 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.1 -name default
config_export -format ip_catalog -rtl verilog
csynth_design
export_design

open_project -reset uram_fifo
open_solution "uram_fifo" -flow_target vivado
set_top uram_fifo
add_files uram_fifo.cpp 
add_files pc_dr.h 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.1 -name default
config_storage fifo -impl uram
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

open_project -reset dma_writer_avg
open_solution "dma_writer_avg" -flow_target vivado
set_top dma_writer_avg
add_files dma_writer_avg.cpp 
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

open_project -reset dropperandfifo
open_solution "dropperandfifo" -flow_target vivado
set_top dropperandfifo
add_files dropperandfifo.cpp 
add_files pc_dr.h 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.1 -name default
config_export -format ip_catalog -rtl verilog
csynth_design
export_design

open_project -reset c_complex8_to_16
open_solution "c_complex8_to_16" -flow_target vivado
set_top c_complex8_to_16
add_files c_complex8_to_16.cpp 
add_files pc_dr.h 
set_part {xczu48dr-ffvg1517-2-e}
create_clock -period 3.1 -name default
config_export -format ip_catalog -rtl verilog
csynth_design
export_design

exit