#include "pc_dr.h"

void dma_writer_avg(
		hls::stream<hls::axis<ap_int<512>, 0, 0, 0>> &in_q,
		ap_int<512> *result_mem,
		int num_samples,
		volatile int* write_in,
		int *write_out
		){

	#pragma HLS INTERFACE axis register port=in_q

	#pragma HLS INTERFACE m_axi port=result_mem bundle=result_mem offset=slave \
		max_read_burst_length=2 max_write_burst_length=64 \
		num_read_outstanding=1 num_write_outstanding=2 \
		latency=60

	#pragma HLS INTERFACE s_axilite register port=num_samples
	#pragma HLS INTERFACE s_axilite register port=write_in
	#pragma HLS INTERFACE s_axilite register port=write_out

	#pragma HLS INTERFACE mode=s_axilite port=return

	static int write_num = 0;

	ap_int<512> buf[64];
	bool writing;

	order:{
		#pragma hls protocol fixed
		for(int cnt2=0; cnt2<64; cnt2++)
			buf[cnt2] = in_q.read().data;
		writing = (*write_in == write_num+1) ? true : false;
	}

	// outer loop to treat all samples
	// inner loops to maximize burst performance
	if(writing){
		all_samples_loop: for(int cnt1=0; cnt1<num_samples/16; cnt1=cnt1+64)
		{
			#pragma HLS loop_flatten off
			#pragma hls pipeline II=128
			#pragma hls dependence dependent=false type=inter variable=result_mem

			// accept
			if(cnt1 > 0)
				for(int cnt2=0; cnt2<64; cnt2++)
					buf[cnt2] = in_q.read().data;
			// save
			for(int cnt2=0; cnt2<64; cnt2++)
				result_mem[cnt1 + cnt2] = buf[cnt2];
		}
		*write_out = write_num+1;
		write_num = write_num+1;
	}else
		// dump until last
		while(!in_q.read().last){}
}
