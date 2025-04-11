#include "hls_print.h"
#include "pc_dr.h"

void dma_writer(
		hls::stream<ap_int<512>> &in_q,
		ap_int<512> *result_mem,
		int num_samples,
		int write_in,
		int *write_out
		){

	#pragma HLS INTERFACE axis register port=in_q

	#pragma HLS INTERFACE m_axi port=result_mem bundle=result_mem offset=slave \
		max_read_burst_length=2 max_write_burst_length=64 \
		num_read_outstanding=1 num_write_outstanding=2

	#pragma HLS INTERFACE s_axilite register port=num_samples
	#pragma HLS INTERFACE s_axilite register port=write_in
	#pragma HLS INTERFACE s_axilite register port=write_out

	#pragma HLS INTERFACE mode=s_axilite port=return

	static int write_num = 0;

	// outer loop to treat all samples
	// inner loops to maximize burst performance
	if(write_in == write_num+1){
		all_samples_loop: for(int cnt1=0; cnt1<num_samples/16; cnt1=cnt1+64)
		{
			#pragma HLS loop_flatten off
			#pragma hls pipeline off

			// accept
			ap_int<512> buf[64];
			accept: for(int cnt2=0; cnt2<64; cnt2++){
				#pragma hls pipeline
				buf[cnt2] = in_q.read();
			}

			// save
			write_result: for(int cnt2=0; cnt2<64; cnt2++){
				#pragma hls pipeline
				result_mem[cnt1 + cnt2] = buf[cnt2];
			}
		}
		*write_out = write_num+1;
		write_num = write_num+1;
	}else{

		// dump until write increases
		dump: for(int cnt2=0; cnt2<64; cnt2++){
			#pragma hls pipeline
			in_q.read();
		}
	}
}
