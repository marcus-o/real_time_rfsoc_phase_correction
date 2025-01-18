Steps to create the phase correction overlay

1. Install Vivado 2022.1, the version matters.
2. download the RFSoC4x2 bord files from https://www.realdigital.org/hardware/rfsoc-4x2
3. install the board files to vivado
Citing this website: https://digilent.com/reference/programmable-logic/guides/installing-vivado-and-vitis:
Open the folder that Vivado was installed into - C:/Xilinx/Vivado or /opt/Xilinx/Vivado by default. Under this folder, navigate to its <version>/data/boards/board_files directory. If this folder doesn't exist, create it.
Copy the folder in the downloaded zip file into this folder.

4. build the vitis_hls ip: for each subfolder in the vitis_hls folder:
- in a console, change the working directory to the subfolder
- to make it a vitis_hls console, run:
```
c:\Xilinx\Vitis_HLS\2022.1\settings64.bat
```
to build the ip using the tcl file in the subfolder, run:
```
vitis_hls build.tcl
```

5. generate the vivado overlay project:
- in a console, change the working directory to the vivado folder
- Clone the pynq repository
	```
	git clone --recursive https://github.com/Xilinx/RFSoC-PYNQ.git
	```
- copy the phase_correction_overlay.tcl to the RFSoC-PYNQ/boards/RFSoC4x2/base folder
	```
	cp phase_correction_overlay.tcl RFSoC-PYNQ/boards/RFSoC4x2/base
	```
- open vivado. in the tcl console, change the working directory to the vivado/RFSoC-PYNQ/boards/RFSoC4x2/base folder

- run the phase_correction_overlay.tcl script
	```
	source phase_correction_overlay.tcl
	```




