#include "pc_dr.h"

void iq_to_hilbert(
		hls::stream<adc_data> &in_sig_i_q,
		hls::stream<adc_data> &in_sig_q_q,
		hls::stream<adc_data_compl> &out_sig_h_q){

	#pragma HLS INTERFACE axis register port = in_sig_i_q
	#pragma HLS INTERFACE axis register port = in_sig_q_q
	#pragma HLS INTERFACE axis register port = out_sig_h_q

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	out_sig_h_q.write(adc_data_compl(in_sig_i_q.read(), in_sig_q_q.read()));
}
