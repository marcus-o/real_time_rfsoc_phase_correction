#include "pc_dr.h"

void c_complex8_to_16(
		hls::stream<adc_data_compl_vec> &in_sig_q,
		hls::stream<adc_data_compl_vec16> &out_sig_h_q){

	#pragma HLS INTERFACE axis register port = in_sig_q
	#pragma HLS INTERFACE axis register port = out_sig_h_q

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=2

	adc_data_compl_vec16 out;

	adc_data_compl_vec in = in_sig_q.read();
	for(int cnt1=0; cnt1<samples_per_clock; cnt1++){
		out[cnt1] = in[cnt1];
	}
	in = in_sig_q.read();
	for(int cnt1=0; cnt1<samples_per_clock; cnt1++){
		out[cnt1+samples_per_clock] = in[cnt1];
	}
	out_sig_h_q.write(out);
}
