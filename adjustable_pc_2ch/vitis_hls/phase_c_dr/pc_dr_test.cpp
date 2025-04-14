#include "pc_dr.h"

#include "math.h"
#include <iostream>
#include <cstring>
#include <fstream>

#include <unistd.h>

#include "hilbert_transform.cpp"
#include "dma_passer.cpp"
#include "measure_worker.cpp"
#include "dma_writer.cpp"
#include "pc_averager.cpp"


int main(){

	int pkg_cnt = 0;
	int num_samples =  13824;

	hls::stream<adc_data_two_val> hilbert_in_q;

	// number of packets to send for test
	int send_packets = num_samples*200;

	// use made up input interferogram stream
	if(0){
		adc_data_two_val val_out;
		int cnt1 = 0;
		for (int cnt=0; cnt<send_packets/2; cnt++){

			fp twopi = fp(2.*3.14);
			fp omega = twopi*fp(0.11);
			fp dur = 20;
			fp_long t1 = fp_long(48000);

			fp_long cnt2 = fp_long(cnt1);
			fp_long cnt3 = fp_long(cnt1+1);
			cnt1 = (cnt1>50000) ? 0 : cnt1 + 2;

			if(((cnt2-t1) < (-dur-1)) | ((cnt2-t1) > (dur+1))){
				adc_data_two_val out;
				out.v1 = 0;
				out.v2 = 0;

				hilbert_in_q.write(out);
			}else{
				adc_data_two_val out;

				fp exp1 = hls::cospi(fp(fp(cnt2-t1)/dur)/fp(2));
				exp1 = exp1*exp1;
				fp phase1 = omega*fp(cnt2-t1);
				fp cos1 = hls::cos(phase1);
				out.v1 = adc_data(fp(8192.) * cos1 * exp1);

				fp exp2 = hls::cospi(fp(fp(cnt3-t1)/dur)/fp(2));
				exp2 = exp2*exp2;
				fp phase2 = omega*fp(cnt3-t1);
				fp cos2 = hls::cos(phase2);
				out.v2 = adc_data(fp(8192.) * cos2 * exp2);

				hilbert_in_q.write(out);
			}
		}
	// use pre-recorded interferogram stream
	}else{
		std::ifstream inputFile("C:/Users/Labor/FPGA/vivado_2022_1/real_time_rfsoc_phase_correction/vitis_hls/phase_c_dr/test_scripts/input.txt", std::ios::binary);
		for(int cnt=0; cnt<send_packets/2; cnt++){
			adc_data_two_val val_out;
			std::string t_string;
			std::string sig_string;
			adc_data in;
			std::getline(inputFile, t_string, ',');
			std::getline(inputFile, sig_string, '\n');
			in = adc_data(50000 * std::stof(sig_string.c_str()));
			val_out.v1 = in;
			std::getline(inputFile, t_string, ',');
			std::getline(inputFile, sig_string, '\n');
			in = adc_data(50000 * std::stof(sig_string.c_str()));
			val_out.v2 = in;
			hilbert_in_q.write(val_out);
		}
		inputFile.close();
	}

	hls::stream<adc_data_compl> hilbert_out_q;
	for(int cnt=0; cnt<send_packets/2; cnt++){
		hilbert_transform(hilbert_in_q, hilbert_out_q);
	}

	hls::stream<adc_data_compl> proc1_to_buf_q;
	hls::stream<adc_data_compl> proc1_from_buf_q;
	hls::stream<adc_data_compl_vec16> out_orig_q;
	hls::stream<adc_data_compl_vec16> out_orig_corrected_q;
	hls::stream<adc_data_double_length_compl_vec8> avg_q;
	hls::stream<correction_data_type> in_correction_data1_q;
	hls::stream<fp_compl> out_meas_ifg_q;
	hls::stream<time_int> out_meas_ifg_time_q;
	pc_dr(
			hilbert_out_q,
			//proc1_to_buf_q, proc1_from_buf_q,
			proc1_to_buf_q, proc1_to_buf_q,
			out_orig_q, out_orig_corrected_q,
			avg_q,
			in_correction_data1_q,
			out_meas_ifg_q,
			out_meas_ifg_time_q
			);

	// the writer of the dma passer has no flow control and would overwrite
	// old data. this is solved by timing in the actual device
	// this is how you would implement it in test if it was possible
	//adc_data_compl_vec16 mem_w[num_samples/16*3];
	//adc_data_compl_vec16 mem_r[num_samples/16*3];
	//dma_passer(proc1_to_buf_q, mem_w, mem_r, 3*num_samples, proc1_from_buf_q);

	hls::stream<log_data_vec4> out_log_data_q;
	measure_worker(
			out_meas_ifg_q,
			out_meas_ifg_time_q,
			in_correction_data1_q,
			out_log_data_q,
			num_samples);
	printf("measured\n");
	measure_worker(
			out_meas_ifg_q,
			out_meas_ifg_time_q,
			in_correction_data1_q,
			out_log_data_q,
			num_samples);
	printf("measured\n");
	measure_worker(
			out_meas_ifg_q,
			out_meas_ifg_time_q,
			in_correction_data1_q,
			out_log_data_q,
			num_samples);
	printf("measured\n");
	fflush(stdout);

	adc_data_double_length_compl_vec8 avg_mem[num_samples/8];
	adc_data_double_length_compl_vec8 result_mem[num_samples/8];
	int demanded_avgs = 33;
	int write_in = 1;
	int write_out = 0;

	int ifg = 0;
	while(write_out==0){
		measure_worker(
				out_meas_ifg_q,
				out_meas_ifg_time_q,
				in_correction_data1_q,
				out_log_data_q,
				num_samples);
		printf("measured\n");
		pc_averager(
				avg_q,
				avg_mem,
				result_mem,
				num_samples,
				demanded_avgs,
				write_in,
				&write_out
				);
		printf("processed\n");
		ifg++;
		printf("write_out: %d \n", write_out);
		printf("ifg_no: %d \n", ifg);
		fflush(stdout);
	}

	// get the result and save to file
	printf("start processing: \r");
	std::ofstream outputFile("C:/Users/Labor/FPGA/vivado_2022_1/real_time_rfsoc_phase_correction/vitis_hls/phase_c_dr/test_scripts/output.txt");
	for(int cnt1=0; cnt1<num_samples/8; cnt1++){
		for(int cnt2=0; cnt2<8; cnt2++){
			outputFile << result_mem[cnt1][cnt2].real().to_int() << std::endl;
			outputFile << result_mem[cnt1][cnt2].imag().to_int() << std::endl;
		}
	}
	outputFile.close();

	while(!hilbert_in_q.empty()) hilbert_in_q.read();
	while(!hilbert_out_q.empty()) hilbert_out_q.read();
	while(!proc1_to_buf_q.empty()) proc1_to_buf_q.read();
	while(!proc1_from_buf_q.empty()) proc1_from_buf_q.read();
	while(!out_orig_q.empty()) out_orig_q.read();
	while(!out_orig_corrected_q.empty()) out_orig_corrected_q.read();
	while(!avg_q.empty()) avg_q.read();
	while(!in_correction_data1_q.empty()) in_correction_data1_q.read();
	while(!out_meas_ifg_q.empty()) out_meas_ifg_q.read();
	while(!out_meas_ifg_time_q.empty()) out_meas_ifg_time_q.read();
	while(!out_log_data_q.empty()) out_log_data_q.read();

	// end gracefully
	return 0;
}
