#include "pc_dr.h"

// volatile prevents burst
// memcpy prevents burst
// burst should be 32768 bits (512*64) or (128*256)

// read write from same location
// https://github.com/Xilinx/Vitis-HLS-Introductory-Examples/blob/master/Interface/Memory/aliasing_axi_master_ports/test.cpp

void data_collector(
		hls::stream<adc_data_compl> &in_q,
		hls::stream<adc_data_compl_vec16> &out_q){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=16

	adc_data_compl_vec16 vec;
	read_vals_from_stream: for(int cnt2=0; cnt2<16; cnt2++){
		#pragma HLS PIPELINE II=1
		#pragma HLS LOOP_TRIPCOUNT min=16 max=16
		vec[cnt2] = in_q.read();
	}
	out_q.write(vec);
}

void dma_writer(
		hls::stream<adc_data_compl_vec16> &in_q,
		adc_data_compl_vec16 *mem,
		int num_samples,
		hls::stream<bool, 300000> &start_q){
	#pragma HLS INTERFACE ap_ctrl_none port=return
	#pragma HLS INLINE OFF

	static bool first = true;

	all_samples_loop_writer: for(int cnt0=0; cnt0<(num_samples)/16; cnt0 = cnt0+64){
		#pragma HLS loop_flatten off
		#pragma hls pipeline off

		adc_data_compl_vec16 buf[64];
		read_vecs_from_stream: for(int cnt1=0; cnt1<64; cnt1++){
			#pragma HLS PIPELINE II=1
			#pragma HLS LOOP_TRIPCOUNT min=64 max=64
			buf[cnt1] = in_q.read();
		}
		write_vecs_to_mem: for(int cnt1=0; cnt1<64; cnt1++){
			#pragma HLS PIPELINE II=1
			#pragma HLS LOOP_TRIPCOUNT min=64 max=64
			mem[cnt0 + cnt1] = buf[cnt1];
		}
		write_start:{
			#pragma HLS PROTOCOL mode=fixed
			//if(first){
				start_q.write(true);
				//first = false;
			//}
		}
	}
}

void dma_reader(
		hls::stream<bool, 300000> &start_q,
		adc_data_compl_vec16 *mem_r,
		int num_samples,
		hls::stream<adc_data_compl_vec16> &out_q){
	#pragma HLS INTERFACE ap_ctrl_none port=return
	#pragma HLS INLINE OFF

	static bool first = true;

	all_samples_loop_reader: for(int cnt0=0; cnt0<(num_samples)/16; cnt0 = cnt0+64){
		#pragma HLS loop_flatten off
		#pragma hls pipeline off

		read_start:{
			#pragma HLS PROTOCOL mode=fixed
			//if(first){
				start_q.read();
				//first = false;
			//}
		}
		adc_data_compl_vec16 buf[64];
		read_vecs_from_mem: for(int cnt1=0; cnt1<64; cnt1++){
			#pragma HLS PIPELINE II=1
			#pragma HLS LOOP_TRIPCOUNT min=64 max=64
			buf[cnt1] = mem_r[cnt0 + cnt1];
		}
		write_vecs_to_stream: for(int cnt1=0; cnt1<64; cnt1++){
			#pragma HLS PIPELINE II=1
			#pragma HLS LOOP_TRIPCOUNT min=64 max=64
			out_q.write(buf[cnt1]);
		}
	}
}

void data_writer(
		hls::stream<adc_data_compl_vec16> &in_q,
		hls::stream<adc_data_compl> &out_q){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=16

	adc_data_compl_vec16 vec = in_q.read();
	write_vals_to_stream: for(int cnt2=0; cnt2<16; cnt2++){
		#pragma HLS PIPELINE II=1
		#pragma HLS LOOP_TRIPCOUNT min=16 max=16
		out_q.write(vec[cnt2]);
	}
}

void dma_passer(
		hls::stream<adc_data_compl> &in_q,
		adc_data_compl_vec16 *mem_w,
		adc_data_compl_vec16 *mem_r,
		int num_samples,
		hls::stream<adc_data_compl> &out_q
		){

	#pragma HLS INTERFACE axis register port=in_q
	#pragma HLS INTERFACE axis register port=out_q
	#pragma HLS INTERFACE s_axilite register port=num_samples

	#pragma HLS INTERFACE m_axi port=mem_w bundle=mem_w offset=slave \
		max_read_burst_length=2 max_write_burst_length=64 \
		num_read_outstanding=1 num_write_outstanding=2

	#pragma HLS INTERFACE m_axi port=mem_r bundle=mem_r offset=slave \
		max_read_burst_length=64 max_write_burst_length=2 \
		num_read_outstanding=2 num_write_outstanding=1

	#pragma HLS ALIAS ports=mem_w, mem_r distance=0

	#pragma HLS INTERFACE s_axilite port=return


	// launch all parallel tasks and their communication
	hls_thread_local hls::stream<adc_data_compl_vec16, 64> sig1_h_q;
	hls_thread_local hls::task t0(data_collector, in_q, sig1_h_q);

	hls_thread_local hls::stream<bool, 300000> start_q;
	hls_thread_local hls::task t1(dma_writer, sig1_h_q, mem_w, num_samples, start_q);

	hls_thread_local hls::stream<adc_data_compl_vec16, 64> sig3_h_q;
	hls_thread_local hls::task t2(dma_reader, start_q, mem_r, num_samples, sig3_h_q);
	hls_thread_local hls::task t3(data_writer, sig3_h_q, out_q);
}

