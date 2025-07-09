
# RFSoC Real-Time Dual-Comb Self-Correction Algorithm
This repository contains a real-time self-correction algorithm for the RFSoC4x2 ( https://www.realdigital.org/hardware/rfsoc-4x2 ) board.
The code should be adaptable to other hardware implementing similar field-programmable gate arrays.
Three codes are provided, one works for a fixed detuning of 20 kHz and 300 MHz bandwidth (retained because it is easier to understand), one for arbitrary detunings (supports two channel correction, adjustable trigger level, low detunings, 300 MHz bandwidth), and one for arbitrary detunings (>= 20 kHz) and very high bandwidth (2.5 GHz).

## Code description
The code and its application are described in our preprint on the arXiv ( https://doi.org/10.48550/arXiv.2503.07005 ).

## Steps for using the code on the RFSoC4x2
Connect to the Jupyter environment on the RFSoC4x2 in your browser and upload the contents of the adjustable_pc/pynq folder.
Install the *.dtbo file to the RFSoC4x2 following the instructions in adjustable_pc/pynq/overlay/insert_dtbo.py.
The provided notebook (adjustable_phase_correction.ipynb) can output
- the full interferogram train with phase correction
- coherently averaged interferograms after phase correction
- the interferogram parameters measured by the phase correction
- the full interferogram train without phase correction

## Connecting the RFSoC4x2
The AMD Zynq Ultrascale+ RFSoC XCZU48DR-2FFVG1517E wants a maximum 0.5 V peak-to-peak voltage at its analog-to-digital converters which are 100-Ohm terminated. 
The RFSoC4x2 inputs convert from 50 Ohm to 100 Ohm using balancing units, thus, we expect the maximally allowed peak-to-peak voltage at the SMA inputs is 0.25 V (i.e., 0.125 V from zero to the pulse maximum).
The provided overlays listen for interferograms at port ADC_D (reference/ main signal) and ADC_C (second signal). For testing, the overlays, after loading, play an infinite interferogram train on the DAC_B output port. To avoid building a radio, DAC_B must always be connected to the ADC_D port or 50-Ohm terminated.

## Authors
Validation, Experimentation, Writing by A. Eber, M. Schultze, B. Bernhardt, M. Ossiander. Code by M. Ossiander.

## License
The code in this repository may be reused under a CC BY-NC 4.0 license ( https://creativecommons.org/licenses/by-nc/4.0/ ).
If used in academic work, please cite our preprint ( https://doi.org/10.48550/arXiv.2503.07005 ) instead of this repository.

## Funding
We acknowledge the AMD University Program for software and hardware support.

The authors gratefully acknowledge support from NAWI Graz.

B.B. acknowledges funding from the European Union (ERC HORIZON EUROPE 947288 ELFIS).

M.O. acknowledges funding from the European Union (grant agreement 101076933 EUVORAM).
The views and opinions expressed are, however, those of the author(s) only and do not necessarily reflect those of the European Union or the European Research Council Executive Agency.
Neither the European Union nor the granting authority can be held responsible for them.

## Understanding the code
Two python scripts in the python_model_code folder illustrate the function of the algorithm and the data flow in the FPGA implementation.
For the FPGA, a phase correction code for a fixed detuning of 20 kHz +- 5% was retained because it is easier to understand. To use its overlay, connect to the Jupyter environment on the RFSoC4x2 in your browser and upload the contents of the fixed_pc/pynq folder. 

## Recreating the phase correction overlays

### For all overlays
1. install Vivado 2022.1, the version matters.

2. install the RFSoC4x2 board files
- download the RFSoC4x2 board files from https://www.realdigital.org/hardware/rfsoc-4x2
- copy the folder from the zip file to the
```
(xilinx install folder)/Vivado/2022.1/data/boards/board_files
```
folder (create the folder if it doesn't exist).

### For creating the fixed detuning phase correction overlay (easier to understand)
3. build the Vitis HLS ip: for each subfolder in the fixed_pc/vitis_hls folder:
- in a console, change the working directory to the subfolder
- to make a vitis_hls console, run (for unix there will be a similar shell script):
```
(xilinx install folder)\Vitis_HLS\2022.1\settings64.bat
```
- to build the ip using the tcl file in the subfolder, run:
```
vitis_hls build.tcl
```

4. generate the vivado overlay project and bitstream:
- open vivado and, in the tcl console, switch the working directory to the vivado directory of this repository, e.g.:
```
cd location/real_time_rfsoc_phase_correction/fixed_pc/vivado/
```
- create the vivado project using the base.tcl file:
```
source base.tcl
```
- click generate the bitstream

4b. generate a vivado test board to simulate if the phase correction works:
- open vivado and, in the tcl console, switch the working directory to the vivado directory of this repository, e.g.:
```
cd location/real_time_rfsoc_phase_correction/fixed_pc/vivado/
```
- create the test board using the test.tcl file:
```
source test.tcl
```
- click simulate, run simulation


### For creating the adjustable detuning phase correction overlay
3. build the Vitis HLS ip: for each subfolder in the adjustable_pc/vitis_hls folder:
- in a console, change the working directory to the subfolder
- to make a vitis_hls console, run (for unix there will be a similar shell script):
```
(xilinx install folder)\Vitis_HLS\2022.1\settings64.bat
```
- to build the ip using the tcl file in the subfolder, run:
```
vitis_hls build.tcl
```

4. generate the vivado overlay project and bitstream:
- open vivado and, in the tcl console, switch the working directory to the vivado directory of this repository, e.g.:
```
cd location/real_time_rfsoc_phase_correction/adjustable_pc/vivado/
```
- create the vivado project using the adj.tcl file:
```
source adj.tcl
```
- click generate the bitstream

### For creating the high bandwidth phase correction overlay
3. build the Vitis HLS ip: for each subfolder in the adjustable_pc_high_speed/vitis_hls folder:
- in a console, change the working directory to the subfolder
- to make a vitis_hls console, run (for unix there will be a similar shell script):
```
(xilinx install folder)\Vitis_HLS\2022.1\settings64.bat
```
- to build the ip using the tcl file in the subfolder, run:
```
vitis_hls build.tcl
```

4. generate the vivado overlay project and bitstream:
- open vivado and, in the tcl console, switch the working directory to the vivado directory of this repository, e.g.:
```
cd location/real_time_rfsoc_phase_correction/adjustable_pc_high_speed/vivado/
```
- create the vivado project using the adj.tcl file:
```
source adj.tcl
```
- in the implementation settings, change the stategy to extra timing opt (this will meet timing for reasonable clock and environmental conditions)
- click generate the bitstream