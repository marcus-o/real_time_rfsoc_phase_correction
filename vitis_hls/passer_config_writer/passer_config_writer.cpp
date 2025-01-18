#include "stdint.h"
#include <ap_axi_sdata.h>
#include "ap_int.h"

#include "passer_config_writer.h"


void passer_config_writer(
		ap_int<32> send,
		hls::stream<config1> &config_out_q
	){

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return

	#pragma HLS INTERFACE s_axilite port=send bundle=a
	#pragma HLS INTERFACE axis register port=config_out_q depth=1

	// #pragma HLS PIPELINE II=1

	static ap_int<32> send_local = 0;

	if(send_local != send){
		send_local = send;
		config1 config_temp;
		config_temp.send = send;
		config_out_q.write(config_temp);
	}
}

