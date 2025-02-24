#include "passer_128_last.h"

void in_data_worker(
		hls::stream<data_packet> &in_data_q,
		hls::stream<data_last, 10> &sig_data_q
		){

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE axis register port=in_data_q
	#pragma HLS INTERFACE ap_fifo register port=sig_data_q
	#pragma HLS PIPELINE II=1 style=frp

	data_packet in = in_data_q.read();
	data_last onwards;
	onwards.data = in.data;
	onwards.last = in.last;
	sig_data_q.write(onwards);
}

void in_config_worker(
		ap_int<32> *send,
		hls::stream<config1, 10> &sig_config_q
		){

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE s_axilite register port=send bundle=a
	#pragma HLS INTERFACE ap_fifo register port=sig_config_q

	static ap_int<32> send_local = 0;

	if(send_local != *send){
		send_local = *send;
		config1 config_temp;
		config_temp.send = *send;
		sig_config_q.write(config_temp);
	}
}

void out_worker(
		hls::stream<data_last, 10> &sig_data_q,
		hls::stream<config1> &sig_config_q,
		hls::stream<data_packet> &out_q
		){

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE ap_fifo register port=sig_data_q
	#pragma HLS INTERFACE ap_fifo register port=sig_config_q
	#pragma HLS INTERFACE axis register port=out_q
	#pragma HLS PIPELINE II=1 style=frp

	static ap_int<32> send_local;

	config1 config_temp;
	if(sig_config_q.read_nb(config_temp))
		send_local = config_temp.send;

	data_last onwards;
	if(sig_data_q.read_nb(onwards))
	{
		data_packet out;
		out.data = onwards.data;
		out.keep = -1;
		out.strb = -1;
		out.last = onwards.last;
		// send packets on if advised to
		if (send_local)
			out_q.write(out);
	}
}

// main function
void passer_128_last(
		hls::stream<data_packet> &in_data_q,
		hls::stream<data_packet> &out_q,
		ap_int<32> *send
		){

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE axis register port=in_data_q
	#pragma HLS INTERFACE axis register port=out_q
	#pragma HLS INTERFACE s_axilite register port=send bundle=a

	// uses threads to allow inserting a 10 sample wide fifo
	// otherwise a stall occurs for the averaged data
	// launch all parallel tasks and their communication
	hls_thread_local hls::stream<data_last, 10> sig_data_q;
	hls_thread_local hls::stream<config1, 10> sig_config_q;
	hls_thread_local hls::task t1(in_data_worker, in_data_q, sig_data_q);
	hls_thread_local hls::task t2(in_config_worker, send, sig_config_q);
	hls_thread_local hls::task t3(out_worker, sig_data_q, sig_config_q, out_q);
}
