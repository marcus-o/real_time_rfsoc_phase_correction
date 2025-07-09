#include "pc_dr.h"

void c12_to_16(
		hls::stream<adc_data_vec12> &in_sig_q,
		hls::stream<adc_data_two_val_vec> &out_sig_h_q){

	#pragma HLS INTERFACE axis register port = in_sig_q
	#pragma HLS INTERFACE axis register port = out_sig_h_q

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	static char swap=0;
	static adc_data_vec12 save;

	adc_data_vec12 in = in_sig_q.read();

	if(swap == 0){
		for(int cnt1=0; cnt1<12; cnt1++){
			save[cnt1] = in[cnt1];
		}
		swap = 1;
	} else if (swap == 1){
		adc_data_vec16 out;
		for(int cnt1=0; cnt1<12; cnt1++){
			out[cnt1] = save[cnt1];
		}
		for(int cnt1=0; cnt1<4; cnt1++){
			out[12+cnt1] = in[cnt1];
		}
		for(int cnt1=0; cnt1<8; cnt1++){
			save[cnt1] = in[4+cnt1];
		}
		adc_data_two_val_vec out2;
		for(int cnt1=0; cnt1<8; cnt1++){
			out2[cnt1].v1 = out[cnt1*2];
			out2[cnt1].v2 = out[cnt1*2+1];
		}
		out_sig_h_q.write(out2);
		swap = 2;
	} else if (swap == 2){
		adc_data_vec16 out;
		for(int cnt1=0; cnt1<8; cnt1++){
			out[cnt1] = save[cnt1];
		}
		for(int cnt1=0; cnt1<8; cnt1++){
			out[8+cnt1] = in[cnt1];
		}
		for(int cnt1=0; cnt1<4; cnt1++){
			save[cnt1] = in[8+cnt1];
		}
		adc_data_two_val_vec out2;
		for(int cnt1=0; cnt1<8; cnt1++){
			out2[cnt1].v1 = out[cnt1*2];
			out2[cnt1].v2 = out[cnt1*2+1];
		}
		out_sig_h_q.write(out2);
		swap = 3;
	} else if (swap == 3){
		adc_data_vec16 out;
		for(int cnt1=0; cnt1<4; cnt1++){
			out[cnt1] = save[cnt1];
		}
		for(int cnt1=0; cnt1<12; cnt1++){
			out[4+cnt1] = in[cnt1];
		}
		adc_data_two_val_vec out2;
		for(int cnt1=0; cnt1<8; cnt1++){
			out2[cnt1].v1 = out[cnt1*2];
			out2[cnt1].v2 = out[cnt1*2+1];
		}
		out_sig_h_q.write(out2);
		swap = 0;
	}
}
