#include "saturate_stream_dr.h"

#include "hls_print.h"

void saturate_stream_dr(
		hls::stream<adc_data_two_val> &out_q
		){
	#pragma HLS PIPELINE II=1
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE axis register port=out_q depth=1

	static int cnt1 = 0;

	const fp twopi = fp(2.*3.14);
	const fp omega = twopi*fp(0.264); // fp(0.11);
	const fp dur = 20;
	const int period = int(614e6/20e3);
	const fp_long t1 = fp_long(period/2);

	fp_long cnt2 = fp_long(cnt1);
	fp_long cnt3 = fp_long(cnt1+1);
	cnt1 = (cnt1>period) ? 0 : cnt1 + 2;

	if(((cnt2-t1) < (-dur-1)) | ((cnt2-t1) > (dur+1))){
		adc_data_two_val out;
		out.v1 = 0;
		out.v2 = 0;

		out_q.write(out);
	}else{
		adc_data_two_val out;

		fp env1 = hls::cospi(fp(fp(cnt2-t1)/dur)/fp(2));
		fp phase1 = omega*fp(cnt2-t1);
		fp cos1 = hls::cos(phase1);
		out.v1 = adc_data(fp(8192.) * cos1 * env1*env1);

		fp env2 = hls::cospi(fp(fp(cnt3-t1)/dur)/fp(2));
		fp phase2 = omega*fp(cnt3-t1);
		fp cos2 = hls::cos(phase2);
		out.v2 = adc_data(fp(8192./2.) * cos2 * env2*env2);

		out_q.write(out);
	}
}

