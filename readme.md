Steps to create the phase correction overlay
(only neccessary to change parameters such as the detuning)

1. install Vivado 2022.1, the version matters.

2. install the RFSoC4x2 board files
- download the RFSoC4x2 board files from https://www.realdigital.org/hardware/rfsoc-4x2
- copy the folder in the downloaded zip file into the <vivado install folder>/<version>/data/boards/board_files folder (create the folder if it doesn't exist)

3. build the vitis_hls ip: for each subfolder in the vitis_hls folder:
- in a console, change the working directory to the subfolder
- to make it a vitis_hls console, run:
```
c:\Xilinx\Vitis_HLS\2022.1\settings64.bat
```
to build the ip using the tcl file in the subfolder, run:
```
vitis_hls build.tcl
```

4. generate the vivado overlay project and bitstream:
- open vivado and switch the working directory to the vivado directory of this repository, e.g.:
```
cd <location>/real_time_rfsoc_phase_correction/vivado/
```
- create the vivado project using the base.tcl file:
```
source base.tcl
```
- generate the bitstream
