
################################################################
# This is a generated script based on design: test_bd
#
# Though there are limitations about the generated script,
# the main purpose of this utility is to make learning
# IP Integrator Tcl commands easier.
################################################################

namespace eval _tcl {
proc get_script_folder {} {
   set script_path [file normalize [info script]]
   set script_folder [file dirname $script_path]
   return $script_folder
}
}
variable script_folder
set script_folder [_tcl::get_script_folder]

################################################################
# Check if script is running in correct Vivado version.
################################################################
set scripts_vivado_version 2022.1
set current_vivado_version [version -short]

if { [string first $scripts_vivado_version $current_vivado_version] == -1 } {
   puts ""
   catch {common::send_gid_msg -ssname BD::TCL -id 2041 -severity "ERROR" "This script was generated using Vivado <$scripts_vivado_version> and is being run in <$current_vivado_version> of Vivado. Please run the script in Vivado <$scripts_vivado_version> then open the design in Vivado <$current_vivado_version>. Upgrade the design by running \"Tools => Report => Report IP Status...\", then run write_bd_tcl to create an updated script."}

   return 1
}

################################################################
# START
################################################################

# To test this script, run the following commands from Vivado Tcl console:
# source test_bd_script.tcl

# If there is no project opened, this script will create a
# project, but make sure you do not have an existing project
# <./myproj/project_1.xpr> in the current working folder.

set list_projs [get_projects -quiet]
if { $list_projs eq "" } {
   create_project project_1 myproj -part xczu48dr-ffvg1517-2-e
   set_property BOARD_PART realdigital.org:rfsoc4x2:part0:1.0 [current_project]
}


# CHANGE DESIGN NAME HERE
variable design_name
set design_name test_bd

# If you do not already have an existing IP Integrator design open,
# you can create a design using the following command:
#    create_bd_design $design_name

# Creating design if needed
set errMsg ""
set nRet 0

set cur_design [current_bd_design -quiet]
set list_cells [get_bd_cells -quiet]

if { ${design_name} eq "" } {
   # USE CASES:
   #    1) Design_name not set

   set errMsg "Please set the variable <design_name> to a non-empty value."
   set nRet 1

} elseif { ${cur_design} ne "" && ${list_cells} eq "" } {
   # USE CASES:
   #    2): Current design opened AND is empty AND names same.
   #    3): Current design opened AND is empty AND names diff; design_name NOT in project.
   #    4): Current design opened AND is empty AND names diff; design_name exists in project.

   if { $cur_design ne $design_name } {
      common::send_gid_msg -ssname BD::TCL -id 2001 -severity "INFO" "Changing value of <design_name> from <$design_name> to <$cur_design> since current design is empty."
      set design_name [get_property NAME $cur_design]
   }
   common::send_gid_msg -ssname BD::TCL -id 2002 -severity "INFO" "Constructing design in IPI design <$cur_design>..."

} elseif { ${cur_design} ne "" && $list_cells ne "" && $cur_design eq $design_name } {
   # USE CASES:
   #    5) Current design opened AND has components AND same names.

   set errMsg "Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 1
} elseif { [get_files -quiet ${design_name}.bd] ne "" } {
   # USE CASES: 
   #    6) Current opened design, has components, but diff names, design_name exists in project.
   #    7) No opened design, design_name exists in project.

   set errMsg "Design <$design_name> already exists in your project, please set the variable <design_name> to another value."
   set nRet 2

} else {
   # USE CASES:
   #    8) No opened design, design_name not in project.
   #    9) Current opened design, has components, but diff names, design_name not in project.

   common::send_gid_msg -ssname BD::TCL -id 2003 -severity "INFO" "Currently there is no design <$design_name> in project, so creating one..."

   create_bd_design $design_name

   common::send_gid_msg -ssname BD::TCL -id 2004 -severity "INFO" "Making design <$design_name> as current_bd_design."
   current_bd_design $design_name

}

common::send_gid_msg -ssname BD::TCL -id 2005 -severity "INFO" "Currently the variable <design_name> is equal to \"$design_name\"."

if { $nRet != 0 } {
   catch {common::send_gid_msg -ssname BD::TCL -id 2006 -severity "ERROR" $errMsg}
   return $nRet
}

set bCheckIPsPassed 1
##################################################################
# CHECK IPs
##################################################################
set bCheckIPs 1
if { $bCheckIPs == 1 } {
   set list_check_ips "\ 
xilinx.com:hls:axi_dump:1.0\
xilinx.com:ip:axi_vip:1.1\
xilinx.com:hls:c_complex8_to_16:1.0\
xilinx.com:hls:dropperandfifo:1.0\
xilinx.com:hls:hilbert_transform:1.0\
xilinx.com:hls:measure_worker:1.0\
xilinx.com:hls:pc_averager:1.0\
xilinx.com:hls:pc_dr:1.0\
xilinx.com:hls:saturate_stream_dr:1.0\
xilinx.com:ip:sim_clk_gen:1.0\
xilinx.com:hls:trigger_worker:1.0\
xilinx.com:hls:uram_fifo:1.0\
"

   set list_ips_missing ""
   common::send_gid_msg -ssname BD::TCL -id 2011 -severity "INFO" "Checking if the following IPs exist in the project's IP catalog: $list_check_ips ."

   foreach ip_vlnv $list_check_ips {
      set ip_obj [get_ipdefs -all $ip_vlnv]
      if { $ip_obj eq "" } {
         lappend list_ips_missing $ip_vlnv
      }
   }

   if { $list_ips_missing ne "" } {
      catch {common::send_gid_msg -ssname BD::TCL -id 2012 -severity "ERROR" "The following IPs are not found in the IP Catalog:\n  $list_ips_missing\n\nResolution: Please add the repository containing the IP(s) to the project." }
      set bCheckIPsPassed 0
   }

}

if { $bCheckIPsPassed != 1 } {
  common::send_gid_msg -ssname BD::TCL -id 2023 -severity "WARNING" "Will not continue with creation of design due to the error(s) above."
  return 3
}

##################################################################
# DESIGN PROCs
##################################################################



# Procedure to create entire design; Provide argument to make
# procedure reusable. If parentCell is "", will use root.
proc create_root_design { parentCell } {

  variable script_folder
  variable design_name

  if { $parentCell eq "" } {
     set parentCell [get_bd_cells /]
  }

  # Get object for parentCell
  set parentObj [get_bd_cells $parentCell]
  if { $parentObj == "" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2090 -severity "ERROR" "Unable to find parent cell <$parentCell>!"}
     return
  }

  # Make sure parentObj is hier blk
  set parentType [get_property TYPE $parentObj]
  if { $parentType ne "hier" } {
     catch {common::send_gid_msg -ssname BD::TCL -id 2091 -severity "ERROR" "Parent <$parentObj> has TYPE = <$parentType>. Expected to be <hier>."}
     return
  }

  # Save current instance; Restore later
  set oldCurInst [current_bd_instance .]

  # Set parent object as current
  current_bd_instance $parentObj


  # Create interface ports

  # Create ports

  # Create instance: axi_dump_0, and set properties
  set axi_dump_0 [ create_bd_cell -type ip -vlnv xilinx.com:hls:axi_dump:1.0 axi_dump_0 ]

  # Create instance: axi_dump_2, and set properties
  set axi_dump_2 [ create_bd_cell -type ip -vlnv xilinx.com:hls:axi_dump:1.0 axi_dump_2 ]

  # Create instance: axi_dump_3, and set properties
  set axi_dump_3 [ create_bd_cell -type ip -vlnv xilinx.com:hls:axi_dump:1.0 axi_dump_3 ]

  # Create instance: axi_dump_4, and set properties
  set axi_dump_4 [ create_bd_cell -type ip -vlnv xilinx.com:hls:axi_dump:1.0 axi_dump_4 ]

  # Create instance: axi_dump_5, and set properties
  set axi_dump_5 [ create_bd_cell -type ip -vlnv xilinx.com:hls:axi_dump:1.0 axi_dump_5 ]

  # Create instance: axi_interconnect_1, and set properties
  set axi_interconnect_1 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_interconnect:2.1 axi_interconnect_1 ]
  set_property -dict [ list \
   CONFIG.NUM_MI {3} \
 ] $axi_interconnect_1

  # Create instance: axi_vip_0, and set properties
  set axi_vip_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:axi_vip:1.1 axi_vip_0 ]
  set_property -dict [ list \
   CONFIG.ADDR_WIDTH {32} \
   CONFIG.ARUSER_WIDTH {0} \
   CONFIG.AWUSER_WIDTH {0} \
   CONFIG.BUSER_WIDTH {0} \
   CONFIG.DATA_WIDTH {32} \
   CONFIG.HAS_BRESP {1} \
   CONFIG.HAS_BURST {0} \
   CONFIG.HAS_CACHE {0} \
   CONFIG.HAS_LOCK {0} \
   CONFIG.HAS_PROT {1} \
   CONFIG.HAS_QOS {0} \
   CONFIG.HAS_REGION {0} \
   CONFIG.HAS_RRESP {1} \
   CONFIG.HAS_WSTRB {1} \
   CONFIG.ID_WIDTH {0} \
   CONFIG.INTERFACE_MODE {MASTER} \
   CONFIG.PROTOCOL {AXI4LITE} \
   CONFIG.READ_WRITE_MODE {READ_WRITE} \
   CONFIG.RUSER_BITS_PER_BYTE {0} \
   CONFIG.RUSER_WIDTH {0} \
   CONFIG.SUPPORTS_NARROW {0} \
   CONFIG.WUSER_BITS_PER_BYTE {0} \
   CONFIG.WUSER_WIDTH {0} \
 ] $axi_vip_0

  # Create instance: c_complex8_to_16_0, and set properties
  set c_complex8_to_16_0 [ create_bd_cell -type ip -vlnv xilinx.com:hls:c_complex8_to_16:1.0 c_complex8_to_16_0 ]

  # Create instance: dropperandfifo_0, and set properties
  set dropperandfifo_0 [ create_bd_cell -type ip -vlnv xilinx.com:hls:dropperandfifo:1.0 dropperandfifo_0 ]

  # Create instance: dropperandfifo_1, and set properties
  set dropperandfifo_1 [ create_bd_cell -type ip -vlnv xilinx.com:hls:dropperandfifo:1.0 dropperandfifo_1 ]

  # Create instance: hilbert_transform_0, and set properties
  set hilbert_transform_0 [ create_bd_cell -type ip -vlnv xilinx.com:hls:hilbert_transform:1.0 hilbert_transform_0 ]

  # Create instance: measure_worker_0, and set properties
  set measure_worker_0 [ create_bd_cell -type ip -vlnv xilinx.com:hls:measure_worker:1.0 measure_worker_0 ]

  # Create instance: pc_averager_0, and set properties
  set pc_averager_0 [ create_bd_cell -type ip -vlnv xilinx.com:hls:pc_averager:1.0 pc_averager_0 ]

  # Create instance: pc_dr_0, and set properties
  set pc_dr_0 [ create_bd_cell -type ip -vlnv xilinx.com:hls:pc_dr:1.0 pc_dr_0 ]

  # Create instance: saturate_stream_dr_0, and set properties
  set saturate_stream_dr_0 [ create_bd_cell -type ip -vlnv xilinx.com:hls:saturate_stream_dr:1.0 saturate_stream_dr_0 ]

  # Create instance: sim_clk_gen_0, and set properties
  set sim_clk_gen_0 [ create_bd_cell -type ip -vlnv xilinx.com:ip:sim_clk_gen:1.0 sim_clk_gen_0 ]
  set_property -dict [ list \
   CONFIG.FREQ_HZ {310000000} \
   CONFIG.RESET_POLARITY {ACTIVE_LOW} \
 ] $sim_clk_gen_0

  # Create instance: trigger_worker_0, and set properties
  set trigger_worker_0 [ create_bd_cell -type ip -vlnv xilinx.com:hls:trigger_worker:1.0 trigger_worker_0 ]

  # Create instance: uram_fifo_0, and set properties
  set uram_fifo_0 [ create_bd_cell -type ip -vlnv xilinx.com:hls:uram_fifo:1.0 uram_fifo_0 ]

  # Create interface connections
  connect_bd_intf_net -intf_net S00_AXI_1 [get_bd_intf_pins axi_interconnect_1/S00_AXI] [get_bd_intf_pins axi_vip_0/M_AXI]
  connect_bd_intf_net -intf_net axi_interconnect_1_M00_AXI [get_bd_intf_pins axi_interconnect_1/M00_AXI] [get_bd_intf_pins pc_averager_0/s_axi_control]
  connect_bd_intf_net -intf_net axi_interconnect_1_M01_AXI [get_bd_intf_pins axi_interconnect_1/M01_AXI] [get_bd_intf_pins measure_worker_0/s_axi_control]
  connect_bd_intf_net -intf_net axi_interconnect_1_M02_AXI [get_bd_intf_pins axi_interconnect_1/M02_AXI] [get_bd_intf_pins trigger_worker_0/s_axi_a]
  connect_bd_intf_net -intf_net c_complex8_to_16_0_out_sig_h_q [get_bd_intf_pins axi_dump_2/in_stream] [get_bd_intf_pins c_complex8_to_16_0/out_sig_h_q]
  connect_bd_intf_net -intf_net dropperandfifo_0_out_q [get_bd_intf_pins dropperandfifo_0/out_q] [get_bd_intf_pins pc_averager_0/in_q]
  connect_bd_intf_net -intf_net dropperandfifo_1_out_q [get_bd_intf_pins axi_dump_3/in_stream] [get_bd_intf_pins dropperandfifo_1/out_q]
  connect_bd_intf_net -intf_net hilbert_transform_0_out_orig_q [get_bd_intf_pins c_complex8_to_16_0/in_sig_q] [get_bd_intf_pins hilbert_transform_0/out_orig_q]
  connect_bd_intf_net -intf_net hilbert_transform_0_out_sig_h_q [get_bd_intf_pins hilbert_transform_0/out_sig_h_q] [get_bd_intf_pins trigger_worker_0/in_q]
  connect_bd_intf_net -intf_net measure_worker_0_out_correction_data_ch_2_q [get_bd_intf_pins axi_dump_0/in_stream] [get_bd_intf_pins measure_worker_0/out_correction_data_ch_2_q]
  connect_bd_intf_net -intf_net measure_worker_0_out_correction_data_q [get_bd_intf_pins measure_worker_0/out_correction_data_q] [get_bd_intf_pins pc_dr_0/in_correction_data1_q]
  connect_bd_intf_net -intf_net measure_worker_0_out_log_data_q [get_bd_intf_pins axi_dump_5/in_stream] [get_bd_intf_pins measure_worker_0/out_log_data_q]
  connect_bd_intf_net -intf_net pc_averager_0_out_q [get_bd_intf_pins axi_dump_4/in_stream] [get_bd_intf_pins pc_averager_0/out_q]
  connect_bd_intf_net -intf_net pc_dr_0_avg_q [get_bd_intf_pins dropperandfifo_0/in_q] [get_bd_intf_pins pc_dr_0/avg_q]
  connect_bd_intf_net -intf_net pc_dr_0_out_meas_ifg_q [get_bd_intf_pins measure_worker_0/in_ifg_q] [get_bd_intf_pins trigger_worker_0/out_meas_ifg_q]
  connect_bd_intf_net -intf_net pc_dr_0_out_meas_ifg_time_q [get_bd_intf_pins measure_worker_0/in_ifg_time_q] [get_bd_intf_pins trigger_worker_0/out_meas_ifg_time_q]
  connect_bd_intf_net -intf_net pc_dr_0_out_orig_corrected_q [get_bd_intf_pins dropperandfifo_1/in_q] [get_bd_intf_pins pc_dr_0/out_orig_corrected_q]
  connect_bd_intf_net -intf_net saturate_stream_dr_0_out_q [get_bd_intf_pins hilbert_transform_0/in_sig_q] [get_bd_intf_pins saturate_stream_dr_0/out_q]
  connect_bd_intf_net -intf_net trigger_worker_0_proc1_buf_out_q [get_bd_intf_pins trigger_worker_0/proc1_buf_out_q] [get_bd_intf_pins uram_fifo_0/in_q]
  connect_bd_intf_net -intf_net uram_fifo_0_out_q [get_bd_intf_pins pc_dr_0/proc1_buf_in_q] [get_bd_intf_pins uram_fifo_0/out_q]

  # Create port connections
  connect_bd_net -net ap_clk_0_1 [get_bd_pins axi_dump_0/ap_clk] [get_bd_pins axi_dump_2/ap_clk] [get_bd_pins axi_dump_3/ap_clk] [get_bd_pins axi_dump_4/ap_clk] [get_bd_pins axi_dump_5/ap_clk] [get_bd_pins axi_interconnect_1/ACLK] [get_bd_pins axi_interconnect_1/M00_ACLK] [get_bd_pins axi_interconnect_1/M01_ACLK] [get_bd_pins axi_interconnect_1/M02_ACLK] [get_bd_pins axi_interconnect_1/S00_ACLK] [get_bd_pins axi_vip_0/aclk] [get_bd_pins c_complex8_to_16_0/ap_clk] [get_bd_pins dropperandfifo_0/ap_clk] [get_bd_pins dropperandfifo_1/ap_clk] [get_bd_pins hilbert_transform_0/ap_clk] [get_bd_pins measure_worker_0/ap_clk] [get_bd_pins pc_averager_0/ap_clk] [get_bd_pins pc_dr_0/ap_clk] [get_bd_pins saturate_stream_dr_0/ap_clk] [get_bd_pins sim_clk_gen_0/clk] [get_bd_pins trigger_worker_0/ap_clk] [get_bd_pins uram_fifo_0/ap_clk]
  connect_bd_net -net ap_rst_n_0_1 [get_bd_pins axi_dump_0/ap_rst_n] [get_bd_pins axi_dump_2/ap_rst_n] [get_bd_pins axi_dump_3/ap_rst_n] [get_bd_pins axi_dump_4/ap_rst_n] [get_bd_pins axi_dump_5/ap_rst_n] [get_bd_pins axi_interconnect_1/ARESETN] [get_bd_pins axi_interconnect_1/M00_ARESETN] [get_bd_pins axi_interconnect_1/M01_ARESETN] [get_bd_pins axi_interconnect_1/M02_ARESETN] [get_bd_pins axi_interconnect_1/S00_ARESETN] [get_bd_pins axi_vip_0/aresetn] [get_bd_pins c_complex8_to_16_0/ap_rst_n] [get_bd_pins dropperandfifo_0/ap_rst_n] [get_bd_pins dropperandfifo_1/ap_rst_n] [get_bd_pins hilbert_transform_0/ap_rst_n] [get_bd_pins measure_worker_0/ap_rst_n] [get_bd_pins pc_averager_0/ap_rst_n] [get_bd_pins pc_dr_0/ap_rst_n] [get_bd_pins saturate_stream_dr_0/ap_rst_n] [get_bd_pins sim_clk_gen_0/sync_rst] [get_bd_pins trigger_worker_0/ap_rst_n] [get_bd_pins uram_fifo_0/ap_rst_n]

  # Create address segments
  assign_bd_address -offset 0x00010000 -range 0x00010000 -target_address_space [get_bd_addr_spaces axi_vip_0/Master_AXI] [get_bd_addr_segs measure_worker_0/s_axi_control/Reg] -force
  assign_bd_address -offset 0x00000000 -range 0x00010000 -target_address_space [get_bd_addr_spaces axi_vip_0/Master_AXI] [get_bd_addr_segs pc_averager_0/s_axi_control/Reg] -force
  assign_bd_address -offset 0x00020000 -range 0x00010000 -target_address_space [get_bd_addr_spaces axi_vip_0/Master_AXI] [get_bd_addr_segs trigger_worker_0/s_axi_a/Reg] -force


  # Restore current instance
  current_bd_instance $oldCurInst

  validate_bd_design
  save_bd_design
}
# End of create_root_design()


##################################################################
# MAIN FLOW
##################################################################

create_root_design ""


