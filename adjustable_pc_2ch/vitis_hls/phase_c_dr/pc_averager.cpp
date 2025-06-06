#include "pc_dr.h"

void pc_averager(
		hls::stream<adc_data_double_length_compl_vec8> &avg_q,
		adc_data_double_length_compl_vec8 *avg_mem,
		adc_data_double_length_compl_vec8 *result_mem,
		int num_samples,
		int demanded_avgs,
		int write_in,
		int *write_out
		){

	#pragma HLS INTERFACE axis register port=avg_q

	#pragma HLS INTERFACE m_axi port=avg_mem bundle=avg_mem offset=slave \
		max_read_burst_length=64 max_write_burst_length=64 \
		num_read_outstanding=2 num_write_outstanding=2
	#pragma HLS INTERFACE m_axi port=result_mem bundle=result_mem offset=slave \
		max_read_burst_length=2 max_write_burst_length=64 \
		num_read_outstanding=1 num_write_outstanding=2

	#pragma HLS INTERFACE s_axilite register port=num_samples
	#pragma HLS INTERFACE s_axilite register port=demanded_avgs
	#pragma HLS INTERFACE s_axilite register port=write_in
	#pragma HLS INTERFACE s_axilite register port=write_out

	#pragma HLS INTERFACE mode=s_axilite port=return

	static int write_num = 0;
	// average
	static int avg_cnt = 0;
	#pragma HLS reset variable=avg_cnt

	// outer loop to treat all samples
	// inner loops to maximize burst performance
	all_samples_loop: for(int cnt1=0; cnt1<num_samples/8; cnt1=cnt1+64)
	{
		#pragma HLS loop_flatten off
		#pragma hls pipeline off

		// load
		adc_data_double_length_compl_vec8 buf[64];
		if(avg_cnt==0){
			init_buf: for(int cnt2=0; cnt2<64; cnt2++){
				#pragma hls pipeline
				buf[cnt2] = adc_data_double_length_compl(0, 0);
			}
		}else{
			load_mem: for(int cnt2=0; cnt2<64; cnt2++){
				#pragma hls pipeline
				buf[cnt2] = avg_mem[cnt1 + cnt2];
			}
		}

		// calc
		calc: for(int cnt2=0; cnt2<64; cnt2++){
			#pragma hls pipeline
			// using vector addition explodes lut use (idk why)
			// loop instead
			adc_data_double_length_compl_vec8 a = avg_q.read();
			for(int cnt3=0; cnt3<8; cnt3++){
				buf[cnt2][cnt3] = a[cnt3] + buf[cnt2][cnt3];
			}
		}

		// save
		if(avg_cnt<(demanded_avgs)-1){
			write_mem: for(int cnt2=0; cnt2<64; cnt2++){
				#pragma hls pipeline
				avg_mem[cnt1 + cnt2] = buf[cnt2];
			}
		}else{
			if(write_in == write_num+1){
				write_result: for(int cnt2=0; cnt2<64; cnt2++){
					#pragma hls pipeline
					result_mem[cnt1 + cnt2] = buf[cnt2];
				}
			}
		}
	}
	if(avg_cnt < (demanded_avgs)-1){
		avg_cnt = avg_cnt+1;
	}else{
		avg_cnt = 0;
		if(write_in == write_num+1){
			*write_out = write_num+1;
			write_num = write_num+1;
		}
	}
}

