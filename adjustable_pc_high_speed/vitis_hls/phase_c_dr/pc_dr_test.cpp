#include "pc_dr.h"

#include "math.h"
#include <iostream>
#include <cstring>
#include <fstream>

#include <unistd.h>

#include "hilbert_transform.cpp"
#include "measure_worker.cpp"
#include "dma_writer.cpp"
#include "pc_averager.cpp"
#include "trigger_worker.cpp"
#include "dropperandfifo.cpp"
#include "c_complex8_to_16.cpp"


int main(){

	// ifg samples at 300MHz
	int num_samples =  13824;

	// number of packets to send for test (at 600 MHz)
	//int send_packets = num_samples*2*180; //2ch
	int send_packets = num_samples*2*240; // acetylene

	hls::stream<adc_data_two_val_vec> hilbert_in_q;
	hls::stream<adc_data_two_val_vec> hilbert_in_q2;
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
				hilbert_in_q2.write(out);
			}
		}
	// use pre-recorded interferogram stream
	}else{
		std::ifstream inputFile("C:/FPGA/real_time_rfsoc_phase_correction/adjustable_pc_high_speed/vitis_hls/phase_c_dr/test_scripts/in_1ch_acetylene.txt", std::ios::binary);
		char step = 0;
		adc_data_two_val_vec vec_out;
		for(int cnt=0; cnt<send_packets/2; cnt++){
			std::string t_string;
			std::string sig_string;
			std::getline(inputFile, t_string, ',');
			std::getline(inputFile, sig_string, '\n');
			vec_out[step].v1 = adc_data(50000 * std::stof(sig_string.c_str()));
			std::getline(inputFile, t_string, ',');
			std::getline(inputFile, sig_string, '\n');
			vec_out[step].v2 = adc_data(50000 * std::stof(sig_string.c_str()));
			step++;
			if(step==8){
				hilbert_in_q.write(vec_out);
				step = 0;
			}
		}
		inputFile.close();

		std::ifstream inputFile2("C:/FPGA/real_time_rfsoc_phase_correction/adjustable_pc_high_speed/vitis_hls/phase_c_dr/test_scripts/in_1ch_acetylene.txt", std::ios::binary);
		step = 0;
		for(int cnt=0; cnt<send_packets/2; cnt++){
			std::string t_string;
			std::string sig_string;
			adc_data in;
			std::getline(inputFile2, t_string, ',');
			std::getline(inputFile2, sig_string, '\n');
			vec_out[step].v1 = adc_data(50000 * std::stof(sig_string.c_str()));
			std::getline(inputFile2, t_string, ',');
			std::getline(inputFile2, sig_string, '\n');
			vec_out[step].v2 = adc_data(50000 * std::stof(sig_string.c_str()));
			step++;
			if(step==8){
				hilbert_in_q2.write(vec_out);
				step = 0;
			}
		}
		inputFile2.close();
	}
	printf("load_data_complete \n\n");
	printf("after hilbert ch1 \n");
	printf("hilbert_in_q: %d \n", hilbert_in_q.size());
	fflush(stdout);

	hls::stream<adc_data_compl_vec> trigger_in_q;
	hls::stream<adc_data_compl_vec> out_orig_8_q;
	hls::stream<adc_data_compl_vec16> out_orig_q;
	for(int cnt=0; cnt<send_packets/16; cnt++){
		hilbert_transform(hilbert_in_q, trigger_in_q, out_orig_8_q);
	}
	while(!out_orig_8_q.empty())
		c_complex8_to_16(out_orig_8_q, out_orig_q);

	printf("after hilbert ch1 \n");
	printf("hilbert_in_q: %d \n", hilbert_in_q.size());
	printf("trigger_in_q: %d \n\n", trigger_in_q.size());
	fflush(stdout);

	hls::stream<adc_data_compl_vec> proc1_to_buf_q;
	hls::stream<adc_data_compl_vec> out_meas_ifg_q;
	hls::stream<time_int> out_meas_ifg_time_q;
	trigger_worker(
			trigger_in_q,
			proc1_to_buf_q,
			out_meas_ifg_q,
			out_meas_ifg_time_q,
			1000*1000
			);
	printf("after trigger ch1 \n");
	printf("trigger_in_q: %d \n", trigger_in_q.size());
	printf("proc1_to_buf_q: %d \n", proc1_to_buf_q.size());
	printf("out_meas_ifg_q: %d \n", out_meas_ifg_q.size());
	printf("out_meas_ifg_time_q: %d \n\n", out_meas_ifg_time_q.size());
	fflush(stdout);

	// the writer of the dma passer has no flow control and would overwrite
	// old data. this is solved by timing in the actual device
	// this is how you would implement it in test if it was possible
	//adc_data_compl_vec16 mem_w[num_samples/16*3];
	//adc_data_compl_vec16 mem_r[num_samples/16*3];
	//dma_passer(proc1_to_buf_q, mem_w, mem_r, 3*num_samples, proc1_from_buf_q);

	hls::stream<correction_data_type> in_correction_data1_q;
	hls::stream<correction_data_type> in_correction_data2_q;
	hls::stream<log_data_vec4> out_log_data_q;
	for(int cnt1=0; cnt1<48; cnt1++){
		measure_worker(
				out_meas_ifg_q,
				out_meas_ifg_time_q,
				in_correction_data1_q,
				in_correction_data2_q,
				out_log_data_q,
				num_samples, 0, 2);
	}
	printf("measured ch1 \n\n");
	printf("out_meas_ifg_q: %d \n", out_meas_ifg_q.size());
	printf("out_meas_ifg_time_q: %d \n", out_meas_ifg_time_q.size());
	printf("in_correction_data1_q: %d \n", in_correction_data1_q.size());
	printf("in_correction_data1_q2: %d \n", in_correction_data2_q.size());
	printf("out_log_data_q: %d \n\n", out_log_data_q.size());
	fflush(stdout);

	printf("writing log_data \n");
	std::ofstream outputFile4("C:/FPGA/real_time_rfsoc_phase_correction/adjustable_pc_high_speed/vitis_hls/phase_c_dr/test_scripts/out_log.txt");
	while(!out_log_data_q.empty()){
		log_data_vec4 out = out_log_data_q.read();
		for(int cnt2=0; cnt2<4; cnt2++){
			outputFile4 << out[cnt2].delta_time_exact << std::endl;
			outputFile4 << out[cnt2].phase << std::endl;
			outputFile4 << out[cnt2].center_freq << std::endl;
			outputFile4 << out[cnt2].phase_change_pi << std::endl;
		}
	}
	outputFile4.close();
	printf("wrote log_data \n\n");
	fflush(stdout);

	int demanded_avgs = 33;

	printf("before correction ch1 \n");
	printf("proc1_to_buf_q: %d \n", proc1_to_buf_q.size());
	printf("in_correction_data1_q: %d \n\n", in_correction_data1_q.size());
	fflush(stdout);

	hls::stream<correction_data_type, 5> in_correction_data1_q2;
	hls::stream<process_data_type, 2> process_q;
	hls::stream<adc_data_compl_vec_info> out_orig_corrected_dropper_q;
	hls::stream<adc_data_compl_vec_info> avg_in_dropper_q;
	hls::stream<process_data_type_reduced, 2> avg_interp_q;
	hls::stream<process_data_type_reduced, 2> orig_corrected_interp_q;
	while(!proc1_to_buf_q.empty()){
		if(in_correction_data1_q.empty() and in_correction_data1_q2.empty())
			break;
		if(in_correction_data1_q2.empty())
			in_correction_data1_q2.write(in_correction_data1_q.read());
		process_worker_primer(
				proc1_to_buf_q,
				in_correction_data1_q2,
				process_q
				);
		process_worker(
				process_q,
				avg_interp_q,
				orig_corrected_interp_q
				);
		avg_interp(
				avg_interp_q,
				avg_in_dropper_q
				);
		orig_corrected_interp(
				orig_corrected_interp_q,
				out_orig_corrected_dropper_q
				);
	}

	printf("after correction ch1 \n");
	printf("proc1_to_buf_q: %d \n", proc1_to_buf_q.size());
	printf("avg_in_dropper_q: %d \n", avg_in_dropper_q.size());
	printf("out_orig_corrected_dropper_q: %d \n", out_orig_corrected_dropper_q.size());
	printf("in_correction_data1_q: %d \n\n", in_correction_data1_q.size());
	fflush(stdout);

	hls::stream<adc_data_compl_vec16> out_orig_corrected_q;
	hls::stream<adc_data_compl_vec16> avg_in_q;

	while(!avg_in_dropper_q.empty())
		out_dropper(avg_in_dropper_q, avg_in_q);
	while(!out_orig_corrected_dropper_q.empty())
		out_dropper(out_orig_corrected_dropper_q, out_orig_corrected_q);

	printf("after dropping ch1 \n");
	printf("avg_in_dropper_q: %d \n", avg_in_dropper_q.size());
	printf("out_orig_corrected_dropper_q: %d \n", out_orig_corrected_dropper_q.size());
	printf("out_orig_q: %d \n", out_orig_q.size());
	printf("out_orig_corrected_q: %d \n", out_orig_corrected_q.size());
	printf("avg_in_q: %d \n", avg_in_q.size());
	fflush(stdout);

	hls::stream<adc_data_double_length_compl_vec8> avg_out_q;
	pc_averager(
		avg_in_q,
		num_samples,
		demanded_avgs,
		avg_out_q
		);

	printf("\n saving averaged stream ch 1: \n");
	std::ofstream outputFile("C:/FPGA/real_time_rfsoc_phase_correction/adjustable_pc_high_speed/vitis_hls/phase_c_dr/test_scripts/out_ref_ch_avg.txt");
	while(!avg_out_q.empty()){
		adc_data_double_length_compl_vec8 out = avg_out_q.read();
		for(int cnt2=0; cnt2<8; cnt2++){
			outputFile << out[cnt2].real().to_int() << std::endl;
			outputFile << out[cnt2].imag().to_int() << std::endl;
		}
	}
	outputFile.close();
	printf("wrote averaged stream ch 1 \n\n");
	fflush(stdout);

	printf("after averaging ch1 \n");
	printf("proc1_to_buf_q: %d \n", proc1_to_buf_q.size());
	printf("out_orig_q: %d \n", out_orig_q.size());
	printf("out_orig_corrected_q: %d \n", out_orig_corrected_q.size());
	printf("avg_in_q: %d \n", avg_in_q.size());
	printf("avg_out_q: %d \n", avg_out_q.size());
	printf("in_correction_data1_q: %d \n\n", in_correction_data1_q.size());
	fflush(stdout);

	printf("saving full corrected stream ch 1: \n");
	std::ofstream outputFile3("C:/FPGA/real_time_rfsoc_phase_correction/adjustable_pc_high_speed/vitis_hls/phase_c_dr/test_scripts/out_orig_corrected.txt");
	while(!out_orig_corrected_q.empty()){
		adc_data_compl_vec16 out = out_orig_corrected_q.read();
		for(int cnt2=0; cnt2<16; cnt2++){
			outputFile3 << out[cnt2].real().to_int() << std::endl;
			outputFile3 << out[cnt2].imag().to_int() << std::endl;
		}
	}
	outputFile3.close();
	printf("wrote full corrected stream ch 1 \n\n");
	fflush(stdout);

	printf("saving full uncorrected stream ch 1: \n");
	std::ofstream outputFile2("C:/FPGA/real_time_rfsoc_phase_correction/adjustable_pc_high_speed/vitis_hls/phase_c_dr/test_scripts/out_orig.txt");
	while(!out_orig_q.empty()){
		adc_data_compl_vec16 out = out_orig_q.read();
		for(int cnt2=0; cnt2<16; cnt2++){
			outputFile2 << out[cnt2].real().to_int() << std::endl;
			outputFile2 << out[cnt2].imag().to_int() << std::endl;
		}
	}
	outputFile2.close();
	printf("wrote full uncorrected stream ch 1 \n\n");
	fflush(stdout);

	while(!out_orig_q.empty()) out_orig_q.read();
	while(!out_orig_corrected_q.empty()) out_orig_corrected_q.read();
	while(!avg_in_q.empty()) avg_in_q.read();
	while(!avg_out_q.empty()) avg_out_q.read();

	return 0;
}
