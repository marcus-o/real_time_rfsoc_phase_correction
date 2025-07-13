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
#include "trigger_worker.cpp"

int main(){

	// ifg samples at 300MHz
	int num_samples =  13824;

	// number of packets to send for test (at 600 MHz)
	// int send_packets = num_samples*2*180; //2ch
	int send_packets = num_samples*2*240; // acetylene

	hls::stream<adc_data_two_val> hilbert_in_q;
	hls::stream<adc_data_two_val> hilbert_in_q2;
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
		std::ifstream inputFile("C:/FPGA/real_time_rfsoc_phase_correction/test_data/in_2ch_ref_short.txt", std::ios::binary);
		for(int cnt=0; cnt<send_packets/2; cnt++){
			adc_data_two_val val_out;
			std::string t_string;
			std::string sig_string;
			adc_data in;
			std::getline(inputFile, sig_string, '\n');
			in = adc_data(std::atoi(sig_string.c_str()));
			val_out.v1 = in;
			std::getline(inputFile, sig_string, '\n');
			in = adc_data(std::atoi(sig_string.c_str()));
			val_out.v2 = in;
			hilbert_in_q.write(val_out);
		}
		inputFile.close();

		std::ifstream inputFile2("C:/FPGA/real_time_rfsoc_phase_correction/test_data/in_2ch_shg_short.txt", std::ios::binary);
		for(int cnt=0; cnt<send_packets/2; cnt++){
			adc_data_two_val val_out;
			std::string t_string;
			std::string sig_string;
			adc_data in;
			std::getline(inputFile2, sig_string, '\n');
			in = adc_data(std::atoi(sig_string.c_str()));
			val_out.v1 = in;
			std::getline(inputFile2, sig_string, '\n');
			in = adc_data(std::atoi(sig_string.c_str()));
			val_out.v2 = in;
			hilbert_in_q2.write(val_out);
		}
		inputFile2.close();
	}
	printf("load_data_complete \n\n");
	fflush(stdout);

	hls::stream<adc_data_compl> hilbert_out_q;
	for(int cnt=0; cnt<send_packets/2; cnt++){
		hilbert_transform(hilbert_in_q, hilbert_out_q);
	}
	printf("after hilbert ch1 \n");
	printf("hilbert_in_q: %d \n", hilbert_in_q.size());
	printf("hilbert_out_q: %d \n\n", hilbert_out_q.size());
	fflush(stdout);

	hls::stream<adc_data_compl> proc1_to_buf_q;
	hls::stream<fp_compl> out_meas_ifg_q;
	hls::stream<time_int> out_meas_ifg_time_q;
	trigger_worker(
			hilbert_out_q,
			proc1_to_buf_q,
			out_meas_ifg_q,
			out_meas_ifg_time_q,
			1000*1000
			);
	printf("after trigger ch1 \n");
	printf("hilbert_out_q: %d \n", hilbert_out_q.size());
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
	hls::stream<correction_data_type> in_correction_data1_q2;
	hls::stream<log_data_vec4> out_log_data_q;
	for(int cnt1=0; cnt1<36; cnt1++){
		measure_worker(
				out_meas_ifg_q,
				out_meas_ifg_time_q,
				in_correction_data1_q,
				in_correction_data1_q2,
				out_log_data_q,
				num_samples, 0, 2);
	}

	printf("measured ch1 \n\n");
	printf("out_meas_ifg_q: %d \n", out_meas_ifg_q.size());
	printf("out_meas_ifg_time_q: %d \n", out_meas_ifg_time_q.size());
	printf("in_correction_data1_q: %d \n", in_correction_data1_q.size());
	printf("in_correction_data1_q2: %d \n", in_correction_data1_q2.size());
	printf("out_log_data_q: %d \n\n", out_log_data_q.size());
	fflush(stdout);

	printf("writing log_data \n");
	std::ofstream outputFile4("C:/FPGA/real_time_rfsoc_phase_correction/adjustable_pc_2ch/vitis_hls/phase_c_dr/test_scripts/out_log.txt");
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

	// pc_dr never runs twice due to internal variables , therefore this script can only save either
	// reference or signal data (need to run twice and change the one here to a zero)
	// save the reference data
	if(1){
		hls::stream<adc_data_compl_vec16> out_orig_q;
		hls::stream<adc_data_compl_vec16> out_orig_corrected_q;
		hls::stream<adc_data_double_length_compl_vec8> avg_q;

		hls::stream<correction_data_type, 5> correction_int_q;
		while(!in_correction_data1_q.empty())
			in_fifo_correction(
				in_correction_data1_q,
				correction_int_q
				);

		hls::stream<process_data_type, 10> primer_out_q;
		while((!proc1_to_buf_q.empty()) and (!correction_int_q.empty()))
			process_worker_primer(
					proc1_to_buf_q,
					correction_int_q,
					primer_out_q
					);

		hls::stream<adc_data_double_length_compl_vec8, 256> avg_int_q;
		hls::stream<adc_data_compl_vec16, 256> out_orig_int_q;
		hls::stream<adc_data_compl_vec16, 256> out_orig_corrected_int_q;
		while(!primer_out_q.empty())
			process_worker(
					primer_out_q,
					avg_int_q,
					out_orig_int_q,
					out_orig_corrected_int_q
					);

		while(!avg_int_q.empty())
			out_fifo_avg(
					avg_int_q,
					avg_q
					);

		while(!out_orig_int_q.empty())
			out_fifo_orig(
					out_orig_int_q,
					out_orig_q
					);

		while(!out_orig_corrected_int_q.empty())
			out_fifo_orig_corrected(
					out_orig_corrected_int_q,
					out_orig_corrected_q
					);

		printf("after correction ch1 \n");
		printf("proc1_to_buf_q: %d \n", proc1_to_buf_q.size());
		printf("out_orig_q: %d \n", out_orig_q.size());
		printf("out_orig_corrected_q: %d \n", out_orig_corrected_q.size());
		printf("avg_q: %d \n", avg_q.size());
		printf("in_correction_data1_q: %d \n\n", in_correction_data1_q.size());
		fflush(stdout);

		adc_data_double_length_compl_vec8 avg_mem[num_samples/8];
		adc_data_double_length_compl_vec8 result_mem[num_samples/8];
		int write_in = 1;
		int write_out = 0;
		int ifg = 0;
		while(not write_out){
			pc_averager(
					avg_q,
					avg_mem,
					result_mem,
					num_samples,
					demanded_avgs,
					write_in,
					&write_out
					);
			ifg++;
			printf("ref: ifg_no: %d, write_out: %d,  \n", ifg, write_out);
			printf("proc1_to_buf_q: %d \n", in_correction_data1_q.size());

			fflush(stdout);
		}
		printf("\n");

		printf("after averaging ch1 \n");
		printf("proc1_to_buf_q: %d \n", proc1_to_buf_q.size());
		printf("out_orig_q: %d \n", out_orig_q.size());
		printf("out_orig_corrected_q: %d \n", out_orig_corrected_q.size());
		printf("avg_q: %d \n", avg_q.size());
		printf("in_correction_data1_q: %d \n\n", in_correction_data1_q.size());
		fflush(stdout);

		printf("\n saving averaged stream ch 1: \n");
		std::ofstream outputFile("C:/FPGA/real_time_rfsoc_phase_correction/adjustable_pc_2ch/vitis_hls/phase_c_dr/test_scripts/out_ref_ch_avg.txt");
		for(int cnt1=0; cnt1<num_samples/8; cnt1++){
			for(int cnt2=0; cnt2<8; cnt2++){
				outputFile << result_mem[cnt1][cnt2].real().to_int() << std::endl;
				outputFile << result_mem[cnt1][cnt2].imag().to_int() << std::endl;
			}
		}
		outputFile.close();
		printf("wrote averaged stream ch 1 \n\n");
		fflush(stdout);

		printf("saving full corrected stream ch 1: \n");
		std::ofstream outputFile3("C:/FPGA/real_time_rfsoc_phase_correction/adjustable_pc_2ch/vitis_hls/phase_c_dr/test_scripts/out_orig_corrected.txt");
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

		return 0;
	}

	// save the signal data

	hls::stream<adc_data_compl> hilbert_out_q2;
	for(int cnt=0; cnt<send_packets/2; cnt++){
		hilbert_transform(hilbert_in_q2, hilbert_out_q2);
	}
	printf("after hilbert ch2 \n");
	printf("hilbert_in_q2: %d \n", hilbert_in_q2.size());
	printf("hilbert_out_q2: %d \n", hilbert_out_q2.size());
	printf("in_correction_data1_q2: %d \n\n", in_correction_data1_q2.size());
	fflush(stdout);

	hls::stream<adc_data_compl_vec16> out_orig_q2;
	hls::stream<adc_data_compl_vec16> out_orig_corrected_q2;
	hls::stream<adc_data_double_length_compl_vec8> avg_q2;

	hls::stream<correction_data_type, 5> correction_int_q2;
	while(!in_correction_data1_q2.empty())
		in_fifo_correction(
			in_correction_data1_q2,
			correction_int_q2
			);

	hls::stream<process_data_type, 10> primer_out_q2;
	while((!hilbert_out_q2.empty()) and (!correction_int_q2.empty()))
		process_worker_primer(
				hilbert_out_q2,
				correction_int_q2,
				primer_out_q2
				);

	hls::stream<adc_data_double_length_compl_vec8, 256> avg_int_q2;
	hls::stream<adc_data_compl_vec16, 256> out_orig_int_q2;
	hls::stream<adc_data_compl_vec16, 256> out_orig_corrected_int_q2;
	while(!primer_out_q2.empty())
		process_worker(
				primer_out_q2,
				avg_int_q2,
				out_orig_int_q2,
				out_orig_corrected_int_q2
				);

	while(!avg_int_q2.empty())
		out_fifo_avg(
				avg_int_q2,
				avg_q2
				);

	while(!out_orig_int_q2.empty())
		out_fifo_orig(
				out_orig_int_q2,
				out_orig_q2
				);

	while(!out_orig_corrected_int_q2.empty())
		out_fifo_orig_corrected(
				out_orig_corrected_int_q2,
				out_orig_corrected_q2
				);

	printf("after correction ch2 \n");
	printf("hilbert_out_q2: %d \n", hilbert_out_q2.size());
	printf("out_orig_q2: %d \n", out_orig_q2.size());
	printf("out_orig_corrected_q2: %d \n", out_orig_corrected_q2.size());
	printf("avg_q2: %d \n", avg_q2.size());
	printf("in_correction_data1_q2: %d \n\n", in_correction_data1_q2.size());
	fflush(stdout);

	adc_data_double_length_compl_vec8 avg_mem2[num_samples/8];
	adc_data_double_length_compl_vec8 result_mem2[num_samples/8];
	int write_in2 = 1;
	int write_out2 = 0;
	int ifg2 = 0;
	while(not write_out2){
		pc_averager(
				avg_q2,
				avg_mem2,
				result_mem2,
				num_samples,
				demanded_avgs,
				write_in2,
				&write_out2
				);
		ifg2++;
		printf("sig: ifg_no2: %d, write_out2: %d,  \n", ifg2, write_out2);
		fflush(stdout);
	}
	printf("\n");

	printf("after averaging ch2 \n");
	printf("hilbert_out_q2: %d \n", hilbert_out_q2.size());
	printf("out_orig_q2: %d \n", out_orig_q2.size());
	printf("out_orig_corrected_q2: %d \n", out_orig_corrected_q2.size());
	printf("avg_q2: %d \n", avg_q2.size());
	printf("in_correction_data1_q2: %d \n\n", in_correction_data1_q2.size());
	fflush(stdout);

	// get the result and save to file
	printf("saving full averaged stream ch 2: \n");
	std::ofstream outputFile2("C:/FPGA/real_time_rfsoc_phase_correction/adjustable_pc_2ch/vitis_hls/phase_c_dr/test_scripts/out_sig_ch_avg.txt");
	for(int cnt1=0; cnt1<num_samples/8; cnt1++){
		for(int cnt2=0; cnt2<8; cnt2++){
			outputFile2 << result_mem2[cnt1][cnt2].real().to_int() << std::endl;
			outputFile2 << result_mem2[cnt1][cnt2].imag().to_int() << std::endl;
		}
	}
	outputFile2.close();
	printf("wrote averaged stream ch 2 \n\n");
	fflush(stdout);

	printf("saving full corrected stream ch 2 \n");
	std::ofstream outputFile3("C:/FPGA/real_time_rfsoc_phase_correction/adjustable_pc_2ch/vitis_hls/phase_c_dr/test_scripts/out_orig_corrected.txt");
	while(!out_orig_corrected_q2.empty()){
		adc_data_compl_vec16 out = out_orig_corrected_q2.read();
		for(int cnt2=0; cnt2<16; cnt2++){
			outputFile3 << out[cnt2].real().to_int() << std::endl;
			outputFile3 << out[cnt2].imag().to_int() << std::endl;
		}
	}
	outputFile3.close();
	printf("wrote full corrected stream ch 1 \n\n");
	fflush(stdout);

	// end gracefully
	return 0;
}
