#include "passer_32_double.h"

void in_data_worker(
		hls::stream<data> &in_data_q,
		hls::stream<data> &in_data_2_q,
		hls::stream<data, 10> &sig_data_q,
		hls::stream<data, 10> &sig_data_2_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1 style=frp

	sig_data_q.write(in_data_q.read());
	sig_data_2_q.write(in_data_2_q.read());
}

void in_config_worker(
		ap_int<32> *send,
		hls::stream<config1, 10> &sig_config_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return

	static ap_int<32> send_local = 0;

	if(send_local != *send){
		send_local = *send;
		config1 config_temp;
		config_temp.send = *send;
		sig_config_q.write(config_temp);
	}
}

void out_worker(
		hls::stream<data, 10> &sig_data_q,
		hls::stream<data, 10> &sig_data_2_q,
		hls::stream<config1> &sig_config_q,
		hls::stream<data> &out_q,
		hls::stream<data> &out_2_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1 style=frp

	static ap_int<32> send_local;

	config1 config_temp;
	if(sig_config_q.read_nb(config_temp))
		send_local = config_temp.send;

	// send packets on if advised to
	data onwards;
	if(sig_data_q.read_nb(onwards))
		if (send_local)
			out_q.write(onwards);
	data onwards2;
	if(sig_data_2_q.read_nb(onwards2))
		if (send_local)
			out_2_q.write(onwards2);
}

// main function
void passer_32_double(
		hls::stream<data> &in_data_q,
		hls::stream<data> &out_q,
		hls::stream<data> &in_data_2_q,
		hls::stream<data> &out_2_q,
		ap_int<32> *send
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE axis register port=in_data_q
	#pragma HLS INTERFACE axis register port=out_q
	#pragma HLS INTERFACE axis register port=in_data_2_q
	#pragma HLS INTERFACE axis register port=out_2_q
	#pragma HLS INTERFACE s_axilite register port=send bundle=a

	// uses threads to allow inserting a 10 sample wide fifo
	// otherwise a stall occurs for the averaged data
	// launch all parallel tasks and their communication
	hls_thread_local hls::stream<data, 10> sig_data_q;
	hls_thread_local hls::stream<data, 10> sig_data_2_q;
	hls_thread_local hls::stream<config1, 10> sig_config_q;
	hls_thread_local hls::task t1(in_data_worker, in_data_q, in_data_2_q, sig_data_q, sig_data_2_q);
	hls_thread_local hls::task t2(in_config_worker, send, sig_config_q);
	hls_thread_local hls::task t3(out_worker, sig_data_q, sig_data_2_q, sig_config_q, out_q, out_2_q);
}
