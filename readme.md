
# RFSoC Real-Time Dual-Comb Self-Phase-Correction Algorithm
This repository contains a real-time self-phase-correction algorithm for the RFSoC4x2 (https://www.realdigital.org/hardware/rfsoc-4x2) board.
The code should be adaptable to other hardware implementing similar field-programmable gate arrays.

## Steps for using the provided overlays on the RFSoC4x2
(real-time phase correction and averaging for a detuning of 20 kHz +- 5%)

Connect to the Jupyter environment on the RFSoC4x2 in your browser and upload the contents of the pynq folder.
The provided notebooks output the full interferogram train with phase correction (phase_correction_full_data.ipynb), coherently averaged interferograms after phase correction (phase_correction_averaging.ipynb), or, for comparison, the full interferogram train without phase correction (no_phase_correction_full_data.ipynb).

## Connecting the RFSoC4x2
The AMD Zynq Ultrascale+ RFSoC XCZU48DR-2FFVG1517E accepts maximally a 1 V peak-to-peak voltage at its analog-to-digital converters which are 100-Ohm terminated. 
The RFSoC4x2 inputs convert from 50 Ohm to 100 Ohm using balancing units, thus, we expect the maximally allowed peak-to-peak voltage at the SMA inputs is 0.5 V (i.e., 0.25 V from zero to the pulse maximum).
The provided overlays listen for interferograms at port ADC_D. For testing, the overlays, after loading, play an infinite interferogram train on the DAC_B output port. To avoid building a radio, DAC_B must always be connected to the ADC_D port or 50-Ohm terminated.

## Steps to create the phase correction overlay
(neccessary to change the detuning)

1. install Vivado 2022.1, the version matters.

2. install the RFSoC4x2 board files
- download the RFSoC4x2 board files from https://www.realdigital.org/hardware/rfsoc-4x2
- copy the folder from the zip file to the
```
(vivado install folder)/(version/data)/boards/board_files
```
folder (create the folder if it doesn't exist).

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
cd location/real_time_rfsoc_phase_correction/vivado/
```
- create the vivado project using the base.tcl file:
```
source base.tcl
```
- generate the bitstream
