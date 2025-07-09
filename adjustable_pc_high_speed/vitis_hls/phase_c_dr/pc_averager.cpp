#include "pc_dr.h"

void pc_averager(
		hls::stream<adc_data_compl_vec16> &in_q,
		int num_samples,
		int demanded_avgs,
		hls::stream<hls::axis<adc_data_double_length_compl_vec8, 0, 0, 0>> &out_q
		){

	#pragma HLS INTERFACE axis register port=in_q
	#pragma HLS INTERFACE axis register port=out_q

	#pragma HLS INTERFACE s_axilite register port=num_samples
	#pragma HLS INTERFACE s_axilite register port=demanded_avgs

	#pragma HLS INTERFACE mode=s_axilite port=return

	adc_data_double_length_compl_vec8 zero;
	for(int cnt3=0; cnt3<8; cnt3++){
		#pragma hls unroll
		zero[cnt3] = adc_data_double_length_compl(0, 0);
	}

	avgs_loop: for(int avg_cnt=0; avg_cnt<demanded_avgs; avg_cnt++){
	#pragma HLS LOOP_FLATTEN off

		adc_data_double_length_compl_vec8 buf[max_ifg_length];
		#pragma HLS AGGREGATE compact=auto variable=buf
		#pragma HLS BIND_STORAGE variable=buf type=ram_s2p impl=uram


		adc_data_compl_vec16 in_data;
		bool swap = true;
		samples_loop: for(int sample_vec_cnt=0; sample_vec_cnt<num_samples/8; sample_vec_cnt++)
		{
			#pragma hls pipeline II=1 style=frp

			int offset = swap ? 0 : 8;
			in_data = swap ? in_q.read() : in_data;
			swap = !swap;

			adc_data_double_length_compl_vec8 old_data = (avg_cnt==0) ? zero : buf[sample_vec_cnt];

			adc_data_double_length_compl_vec8 new_data;
			for(int cnt3=0; cnt3<8; cnt3++){
				#pragma hls unroll
				new_data[cnt3] = adc_data_double_length_compl(adc_data_double_length(in_data[cnt3+offset].real()), adc_data_double_length(in_data[cnt3+offset].imag())) + old_data[cnt3];
			}

			// save
			if(avg_cnt==(demanded_avgs)-1){  // last avg
				hls::axis<adc_data_double_length_compl_vec8, 0, 0, 0> packet;
				packet.data = new_data;
				packet.keep = -1;
				packet.strb = -1;
				packet.last = (sample_vec_cnt==num_samples/8-1) ? 1 : 0;
				out_q.write(packet); // send data
			}else{
				buf[sample_vec_cnt] = new_data; // otherwise store data
			}
		}
	}
}
