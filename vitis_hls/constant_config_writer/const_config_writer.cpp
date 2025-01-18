#include "stdint.h"
#include <ap_axi_sdata.h>
#include "ap_int.h"

#include "const_config_writer.h"


void const_config_writer(
		hls::stream<ap_int<32>> &config_out_q
	){

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE axis register port=config_out_q depth=1

	// #pragma HLS PIPELINE II=1
	config_out_q.write(10);
}

