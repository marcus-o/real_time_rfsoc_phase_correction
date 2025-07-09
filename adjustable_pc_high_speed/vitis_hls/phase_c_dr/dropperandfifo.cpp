#include "pc_dr.h"

void out_dropper(
		hls::stream<adc_data_compl_vec_info> &in_q,
		hls::stream<adc_data_compl_vec16> &out_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1 style=frp

	static int out_stage = 0;
	static ap_int<512> out;

	adc_data_compl_vec_info in = in_q.read();
	adc_data_compl_vec in_val = in.data;
	int no_samples = in.no_samples;

	if(no_samples>0)
	{
		// cast bit by bit (i cant believe this is more efficient than reinterpret cast)
		//ap_int<512> temp = reinterpret_cast<ap_int<512>&>(in_val);
		ap_int<512> temp = 0;
		for(int cnt = 0; cnt<samples_per_clock; cnt++){
			#pragma HLS UNROLL
			temp.range(cnt*32+15, cnt*32) = in_val[cnt].real().range(15, 0);
			temp.range(cnt*32+16+15, cnt*32+16) = in_val[cnt].imag().range(15, 0);
		}

		// sort samples into this and next packet
		ap_int<512> out_next = 0;
		if(out_stage + no_samples <= 16){
			out.range(out_stage*32 + no_samples*32 - 1, out_stage*32) = temp.range(no_samples*32-1, 0);
		} else {
			int s1 = 16 - out_stage;
			out.range(out_stage*32 + s1*32 -1, out_stage*32) = temp.range(s1*32-1, 0);
			out_next.range((no_samples-s1)*32-1, 0) = temp.range(no_samples*32-1, s1*32);
		}

		// send if this packet is full (16 samples are available)
		if(out_stage + no_samples >= 16){
			// cast back bit by bit (i cant believe this is more efficient than reinterpret cast)
			//adc_data_compl_vec16 temp2 = reinterpret_cast<adc_data_compl_vec16&>(out);
			adc_data_compl_vec16 temp2;
			for(int cnt = 0; cnt<16; cnt++){
				#pragma HLS UNROLL
				temp2[cnt] = adc_data_compl(adc_data(out.range(cnt*32+15, cnt*32)), adc_data(out.range(cnt*32+16+15, cnt*32+16)));
			}
			out_q.write(temp2);
		}
		out = out_stage + no_samples >= 16 ? out_next : out;
		out_stage = out_stage + no_samples >= 16 ? out_stage + no_samples - 16 : out_stage + no_samples;
	}
}

void out_fifo(
		hls::stream<adc_data_compl_vec16, 256> &in_q,
		hls::stream<adc_data_compl_vec16> &out_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	out_q.write(in_q.read());
}

// main function
void dropperandfifo(
		hls::stream<adc_data_compl_vec_info> &in_q,
		hls::stream<adc_data_compl_vec16> &out_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE axis register port = in_q
	#pragma HLS INTERFACE axis register port = out_q

	// launch all parallel tasks and their communication
	hls_thread_local hls::stream<adc_data_compl_vec16, 256> fifo_q;
	hls_thread_local hls::task t1(out_dropper, in_q, fifo_q);
	hls_thread_local hls::task t2(out_fifo, fifo_q, out_q);
}
