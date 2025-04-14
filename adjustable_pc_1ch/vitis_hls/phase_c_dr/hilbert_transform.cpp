#include "pc_dr.h"

// hilbert FIR filter parameters and response
const int hilbert_numtaps = 101;
const int hilbert_mid = (hilbert_numtaps - 1) / 2;  // Half of the number of taps
const fp fir_coeffs_hilbert[hilbert_numtaps] = {
	0.        , -0.01299224,  0.        , -0.0135451 ,  0.        ,
	-0.01414711,  0.        , -0.01480511,  0.        , -0.01552731,
	0.        , -0.01632358,  0.        , -0.01720594,  0.        ,
	-0.01818914,  0.        , -0.01929151,  0.        , -0.02053612,
	0.        , -0.02195241,  0.        , -0.02357851,  0.        ,
	-0.02546479,  0.        , -0.02767912,  0.        , -0.03031523,
	0.        , -0.0335063 ,  0.        , -0.03744822,  0.        ,
	-0.04244132,  0.        , -0.04897075,  0.        , -0.05787452,
	0.        , -0.07073553,  0.        , -0.09094568,  0.        ,
	-0.12732395,  0.        , -0.21220659,  0.        , -0.63661977,
	0.        ,  0.63661977,  0.        ,  0.21220659,  0.        ,
	0.12732395,  0.        ,  0.09094568,  0.        ,  0.07073553,
	0.        ,  0.05787452,  0.        ,  0.04897075,  0.        ,
	0.04244132,  0.        ,  0.03744822,  0.        ,  0.0335063 ,
	0.        ,  0.03031523,  0.        ,  0.02767912,  0.        ,
	0.02546479,  0.        ,  0.02357851,  0.        ,  0.02195241,
	0.        ,  0.02053612,  0.        ,  0.01929151,  0.        ,
	0.01818914,  0.        ,  0.01720594,  0.        ,  0.01632358,
	0.        ,  0.01552731,  0.        ,  0.01480511,  0.        ,
	0.01414711,  0.        ,  0.0135451 ,  0.        ,  0.01299224,
	0.        };

// hilbert_transform sends 50 zeros before it starts sending the signal
// this will transfer a stream of 2 real samples per clock to
// one complex sample per clock

void hilbert_transform(
		hls::stream<adc_data_two_val> &in_sig_q,
		hls::stream<adc_data_compl> &out_sig_h_q){

	#pragma HLS INTERFACE axis register port = in_sig_q
	#pragma HLS INTERFACE axis register port = out_sig_h_q

	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	const int buf_size0 = 51;
	const int hilbert_mid_adj = 25;

	static adc_data_two_val buf[buf_size0];
	#pragma HLS array_partition variable=buf type=complete
	#pragma HLS reset variable=buf off

	// used to multiply with fmax/2: cnt0*2, cnt1*0, cnt2*-2, cnt3*0
	static single_digit swap = 2;
	#pragma HLS reset variable=swap

	// shift by one
	for(int cnt=0; cnt<buf_size0-1; cnt++){
		#pragma hls unroll
		buf[cnt] = buf[cnt+1];
	}
	// fill last position
	buf[buf_size0-1] = in_sig_q.read();;

	adc_data mid_val = buf[hilbert_mid_adj].v1;
	fp sum_temp = 0;
	for(int cnt=0; cnt<buf_size0; cnt++){
		#pragma hls unroll
		sum_temp -= fp(buf[cnt].v2) * fir_coeffs_hilbert[cnt*2+1];
	}
	out_sig_h_q.write(adc_data_compl(mid_val*swap, adc_data(sum_temp)*swap));
	swap = -1*swap;
}
