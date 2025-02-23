#include "passer_128_last.h"

/*
void passer_128_last(
		hls::stream<data_packet> &in_q,
		hls::stream<data_packet> &out_q,
		hls::stream<config1> &config_in_q
		){

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return

	#pragma HLS INTERFACE axis register port=in_q depth=1
	#pragma HLS INTERFACE axis register port=out_q depth=1
	#pragma HLS INTERFACE axis register port=config_in_q depth=1

	#pragma HLS PIPELINE II=1

	static ap_int<32> send_local;

	// all reads and writes
	data_packet in = in_q.read();

	config1 config_temp;
	if(config_in_q.read_nb(config_temp))
		send_local = config_temp.send;

	// send packets on if advised to
	if (send_local)
		out_q.write(in);
}*/

void in_worker(
		hls::stream<data_packet> &in_q,
		hls::stream<data, 10> &out_q,
		hls::stream<bool, 10> &out_q_last
		){

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE axis register port=in_q depth=2
	#pragma HLS INTERFACE ap_fifo register port=out_q depth=10
	#pragma HLS INTERFACE ap_fifo register port=out_q_last depth=10
	#pragma HLS PIPELINE II=1 style=frp

	data_packet in = in_q.read();
	out_q.write(in.data);
	out_q_last.write(in.last);
}

void out_worker(
		hls::stream<data, 10> &in_q,
		hls::stream<bool, 10> &in_q_last,
		hls::stream<data_packet> &out_q,
		hls::stream<config1> &config_in_q
		){

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE ap_fifo register port=in_q depth=10
	#pragma HLS INTERFACE ap_fifo register port=in_q_last depth=10
	#pragma HLS INTERFACE axis register port=out_q depth=2
	#pragma HLS INTERFACE axis register port=config_in_q depth=2
	#pragma HLS PIPELINE II=1 style=frp

	static ap_int<32> send_local;

	// all reads and writes
	data_packet in;
	in.data = in_q.read();
	in.keep = -1;
	in.strb = -1;
	in.last = in_q_last.read();

	config1 config_temp;
	if(config_in_q.read_nb(config_temp))
		send_local = config_temp.send;

	// send packets on if advised to
	if (send_local)
		out_q.write(in);
}

// main function
void passer_128_last(
		hls::stream<data_packet> &in_q,
		hls::stream<data_packet> &out_q,
		hls::stream<config1> &config_in_q
		){

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE axis register port=in_q depth=1
	#pragma HLS INTERFACE axis register port=out_q depth=1
	#pragma HLS INTERFACE axis register port=config_in_q depth=1

	// launch all parallel tasks and their communication
	hls_thread_local hls::stream<data, 10> sig_q;
	hls_thread_local hls::stream<bool, 10> sig_q_last;
	hls_thread_local hls::task t1(in_worker, in_q, sig_q, sig_q_last);
	hls_thread_local hls::task t2(out_worker, sig_q, sig_q_last, out_q, config_in_q);

}
