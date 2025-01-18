#include "pc_dr.h"
#include "math.h"
#include <iostream>
#include <cstring>
#include <fstream>

int main(){

	hls::stream<adc_data_two_val> in_q;
	hls::stream<adc_data_double_length_compl_2sampl_packet> out_q;
	hls::stream<ap_int<32>> avgs_q;
	hls::stream<log_data_packet> log_q;
	int pkg_cnt = 0;

    printf("retain pkg: %d \n", retain_samples);

    // start processing
	pc_dr(in_q, out_q, avgs_q, log_q);

	// set averaging
	int avgs = 33;
	avgs_q.write(avgs);

	// number of packets to send for test
	const int send_packets = retain_samples*(100+40);

	// use made up input interferogram stream
	if(0){
		adc_data_two_val val_out;
		int cnt1 = 0;
		for (int cnt=0; cnt<send_packets/2; cnt++){

			const fp twopi = fp(2.*3.14);
			const fp omega = twopi*fp(0.11);
			const fp dur = 20;
			const fp_long t1 = fp_long(48000);

			fp_long cnt2 = fp_long(cnt1);
			fp_long cnt3 = fp_long(cnt1+1);
			cnt1 = (cnt1>50000) ? 0 : cnt1 + 2;

			if(((cnt2-t1) < (-dur-1)) | ((cnt2-t1) > (dur+1))){
				adc_data_two_val out;
				out.v1 = 0;
				out.v2 = 0;

				in_q.write(out);
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

				in_q.write(out);
			}
		}
	// use pre-recorded interferogram stream
	}else{
		std::ifstream inputFile("C:/Users/Labor/FPGA/vivado_2022_1/vitis_hls/test_python/input.txt", std::ios::binary);
		bool swap=true;
		adc_data_two_val val_out;
	    for(int cnt=0; cnt<send_packets; cnt++){
		   std::string t_string;
		   std::string sig_string;
		   std::getline(inputFile, t_string, ',');
		   std::getline(inputFile, sig_string, '\n');
		   adc_data in = adc_data(50000 * std::stof(sig_string.c_str()));
		   if(swap){
			  val_out.v1 = in;
		   }else{
			   val_out.v2 = in;
			   in_q.write(val_out);
		   }
		   swap = !swap;
		}
		inputFile.close();
    }

	// get the result and save to file
	printf("start processing: \r");
	std::ofstream outputFile("C:/Users/Labor/FPGA/vivado_2022_1/vitis_hls/test_python/output.txt");
    while(pkg_cnt<retain_samples/2){
    	adc_data_double_length_compl_2sampl_packet out_packet;
        if (out_q.read_nb(out_packet)){
        	pkg_cnt++;
        	adc_data_double_length_compl_2sampl out = out_packet.data;  //.v1;
        	outputFile << out.v1.real().to_int() << std::endl;
        	outputFile << out.v1.imag().to_int() << std::endl;
        	outputFile << out.v2.real().to_int() << std::endl;
        	outputFile << out.v2.imag().to_int() << std::endl;
            printf("pkg: %d, %d, %d \n", int(pkg_cnt), out.v1.real().to_int(), out_packet.last);
        }
    }
	outputFile.close();

	// how many packets were received?
    printf("pkg_cnt:%d \n", int(pkg_cnt));

    // end gracefully
    return 0;
}
