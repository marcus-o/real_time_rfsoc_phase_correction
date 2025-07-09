#include "pc_dr.h"

void uram_fifo_in(
		hls::stream<adc_data_compl_vec> &in_q,
		hls::stream<adc_data_compl_vec, wait_buf_size> &between_q
		){
	#pragma HLS PIPELINE II=1
	between_q.write(in_q.read());
}

void uram_fifo_out(
		hls::stream<adc_data_compl_vec, wait_buf_size> &between_q,
		hls::stream<adc_data_compl_vec> &out_q
		){
	#pragma HLS PIPELINE II=1
	out_q.write(between_q.read());
}

void uram_fifo(
	hls::stream<adc_data_compl_vec> &in_q,
	hls::stream<adc_data_compl_vec> &out_q
	){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return

	#pragma HLS INTERFACE axis register port=in_q
	#pragma HLS INTERFACE axis register port=out_q

	hls_thread_local hls::stream<adc_data_compl_vec, wait_buf_size> between_q;
	hls_thread_local hls::task t1(uram_fifo_in, in_q, between_q);
	hls_thread_local hls::task t2(uram_fifo_out, between_q, out_q);

}
