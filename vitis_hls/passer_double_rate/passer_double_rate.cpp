#include "passer_double_rate.h"

void passer_double_rate(
		hls::stream<adc_data> &in_q,
		hls::stream<adc_data> &out_q,
		hls::stream<config1> &config_in_q
		){

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return

	#pragma HLS INTERFACE axis register port=in_q depth=1
	#pragma HLS INTERFACE axis register port=out_q depth=1
	#pragma HLS INTERFACE axis register port=config_in_q depth=1

	#pragma HLS PIPELINE II=1

	static ap_int<32> send_local;

	// all reads and writes
	adc_data val_in = in_q.read();

	config1 config_temp;
	if(config_in_q.read_nb(config_temp))
		send_local = config_temp.send;

	// send packets on if advised to
	if (send_local)
		out_q.write(val_in);
}

