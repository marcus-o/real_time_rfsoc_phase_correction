#include "pc_dr.h"

void c8_to_16(
		hls::stream<adc_data_vec> &in_sig_q,
		hls::stream<adc_data_two_val_vec> &out_sig_h_q){

	#pragma HLS INTERFACE axis register port = in_sig_q
	#pragma HLS INTERFACE axis register port = out_sig_h_q

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=2

	adc_data_two_val_vec out;

	adc_data_vec in = in_sig_q.read();
	for(int cnt1=0; cnt1<4; cnt1++){
		out[cnt1].v1 = in[cnt1*2];
		out[cnt1].v2 = in[cnt1*2+1];
	}
	in = in_sig_q.read();
	for(int cnt1=0; cnt1<4; cnt1++){
		out[cnt1+4].v1 = in[cnt1*2];
		out[cnt1+4].v2 = in[cnt1*2+1];
	}
	out_sig_h_q.write(out);
}
