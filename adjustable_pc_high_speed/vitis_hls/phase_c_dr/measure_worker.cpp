#include "pc_dr.h"

fp abs_sq(fp_compl_short a){
	#pragma HLS inline
	return a.real()*a.real() + a.imag()*a.imag();
	}

const int buf_size2 = 2*((size_ifg_2 + size_spec_2));
const int buf_size2vec = 2*((size_ifg_2 + size_spec_2)/samples_per_clock);

void fft(fp_compl ifg_in1[buf_size2], fp_compl_short spec_ifg[2*size_spec_2], int shift) {
	#pragma HLS dataflow

	fp_compl_short xn[2*size_spec_2];
	fp_compl_short xk[2*size_spec_2];
	config_t fft_config1;
	status_t fft_status1;

	fft_config1.setDir(1);
	send_to_fft: for(int idx=0; idx<2*size_spec_2; idx++){
		#pragma HLS pipeline II=1 rewind
		fp_compl temp = ifg_in1[idx + shift]*fp_compl(fp(1/65536.), 0.);
		xn[idx] = fp_compl_short(fp_short(temp.real()), fp_short(temp.imag()));
	}
    hls::fft<param1>(xn, xk, &fft_status1, &fft_config1);
	read_from_fft: for (int idx=0; idx<2*size_spec_2; idx++){
		#pragma HLS pipeline II=1 rewind
		spec_ifg[idx] = xk[idx];
	}
}

void measure_worker(
		hls::stream<adc_data_compl_vec> &in_ifg_q,
		hls::stream<time_int> &in_ifg_time_q,
		hls::stream<correction_data_type> &out_correction_data_q,
		hls::stream<correction_data_type> &out_correction_data_ch_2_q,
		hls::stream<log_data_vec4> &out_log_data_q,
		int num_samples,
		int delay_ch_2,
		int phase_mult_ch_2
		){
	#pragma HLS INTERFACE axis register port = in_ifg_q
	#pragma HLS INTERFACE axis register port = in_ifg_time_q
	#pragma HLS INTERFACE axis register port = out_correction_data_q
	#pragma HLS INTERFACE axis register port = out_correction_data_ch_2_q
	#pragma HLS INTERFACE axis register port = out_log_data_q

	#pragma HLS INTERFACE s_axilite register port=num_samples
	#pragma HLS INTERFACE s_axilite register port=delay_ch_2
	#pragma HLS INTERFACE s_axilite register port=phase_mult_ch_2

	#pragma HLS INTERFACE mode=s_axilite port=return

	int min_f_idx = 0;
	int max_f_idx = size_spec_2;

	static time_int center_time_prev = 0;
	#pragma HLS reset variable=center_time_prev
	static fp_time center_time_prev_exact = 0;
	#pragma HLS reset variable=center_time_prev_exact
	static float offset_time0 = 0;
	#pragma HLS reset variable=offset_time0
	static float center_phase_prev_pi = 0;
	#pragma HLS reset variable=center_phase_prev_pi
	static float phase_change_prev_pi = 0;
	#pragma HLS reset variable=phase_change_prev_pi
	static float center_freq0 = 0;
	#pragma HLS reset variable=center_freq0

	static int cnt_proc_loops = 0;
	#pragma HLS reset variable=cnt_proc_loops

	static log_data_stage stage_log=0;
	#pragma HLS reset variable=stage_log
	static log_data_vec4 out_log;
	#pragma HLS reset variable=out_log

	// receive
	fp_compl ifg_in1[buf_size2];
	time_int t_in1[buf_size2];
	time_int start_time = in_ifg_time_q.read();
	receive: for(int idx=0; idx<buf_size2vec; idx++){
		#pragma HLS pipeline II=1 style=frp
		adc_data_compl_vec temp = in_ifg_q.read();
		for(char cnt=0; cnt<samples_per_clock; cnt++){
			#pragma HLS unroll
			ifg_in1[idx*samples_per_clock+cnt] = fp_compl(
				fp(temp[cnt].real()),
				fp(temp[cnt].imag()));
			t_in1[idx*samples_per_clock+cnt] = start_time + idx*samples_per_clock + cnt;
		}
	}

	// measure time
	fp_long proc1_sum1 = 0;
	fp_long proc1_sum2 = 0;
	measure_center_time: for(int idx=size_spec_2; idx<size_spec_2 + 2*size_ifg_2; idx++){
		#pragma HLS pipeline II=1
		fp abs_ifg_temp = abs_sq(fp_compl_short(
				fp_short(ifg_in1[idx].real()*fp(1/65536.)),
				fp_short(ifg_in1[idx].imag()*fp(1/65536.))));
		proc1_sum1 += abs_ifg_temp * fp(idx-size_spec_2);
		proc1_sum2 += abs_ifg_temp;
	}

	int shift = size_ifg_2;
	fp_time center_time_observed_exact = fp_time(t_in1[size_spec_2+size_ifg_2]);
	if(proc1_sum2 > 0){
		shift = fp(proc1_sum1 / proc1_sum2).to_int();
		center_time_observed_exact = fp_time(t_in1[size_spec_2]) + proc1_sum1 / proc1_sum2;
	}
	time_int center_time_observed = t_in1[size_spec_2+shift];

	// measure phase
	float center_phase_observed_pi = hls::atan2pi(
			ifg_in1[size_spec_2 + shift].imag(),
			ifg_in1[size_spec_2 + shift].real()).to_float();

	//calculate spectrum
	fp_long proc2_sum1;
	fp_long proc2_sum2;

	fp_compl_short spec_ifg[2*size_spec_2];
	fft(ifg_in1, spec_ifg, shift);

	// measure central frequency
	// must treat both pos and neg frequencies
	fp_long proc2_sum1_pos = 0;
	fp_long proc2_sum2_pos = 0;
	measure_pos_freq: for (int idx=min_f_idx; idx<max_f_idx; idx++){
		#pragma HLS pipeline II=1 rewind
		fp abs_spec_ifg_temp = abs_sq(spec_ifg[idx]); // does not have to be scaled as it is scaled before fft /fp_long(65536);
		proc2_sum1_pos += fp_long(abs_spec_ifg_temp * fp(idx));
		proc2_sum2_pos += fp_long(abs_spec_ifg_temp);
	}
	fp_long proc2_sum1_neg = 0;
	fp_long proc2_sum2_neg = 0;
	measure_neg_freq: for (int idx=max_f_idx; idx<2*max_f_idx; idx++){
		#pragma HLS pipeline II=1 rewind
		fp abs_spec_ifg_temp = abs_sq(spec_ifg[idx]); // does not have to be scaled as it is scaled before fft /fp_long(65536);
		proc2_sum1_neg += fp_long(abs_spec_ifg_temp * fp(idx - 2*max_f_idx));
		proc2_sum2_neg += fp_long(abs_spec_ifg_temp);
	}
	proc2_sum1 = proc2_sum1_pos + proc2_sum1_neg;
	proc2_sum2 = proc2_sum2_pos + proc2_sum2_neg;

	float center_freq_observed = 0;
	if(proc2_sum2 > fp_long(0))
		center_freq_observed = proc2_sum1.to_float() / proc2_sum2.to_float() / float(size_spec_2*2); // / fp_small_long(t_unit);

	// first interferogram defines the frequency for all of them
	if (cnt_proc_loops == 0)
		center_freq0 = center_freq_observed;

	// first and second interferogram define the sampling rate
	// we want an exact delta for interpüolation but
	// an integer delta for phase correction
	float delta_time = (center_time_observed - center_time_prev).to_float();
	float delta_time_exact = (center_time_observed_exact - center_time_prev_exact).to_float();

	if(cnt_proc_loops == 1)
		offset_time0 = delta_time;

	// slope of the phase needed to shift frequency to zero
	float phase_slope_pi = center_freq0 * 2.; //* fp_small_long(t_unit)

	// two ways to do the phase unwrap
	float phase_change_pi = 0;
	float full_phase_slope_pi;
	// unwrap using the phase change
	if(0){
		// calculate the leftover phase at the next ifg and correct
		// first, predict phase correction for the entire ifg-ifg distance
		float phase_applied_ps_pi = phase_slope_pi * delta_time;
		// how much additional phase do we need?
		float add_phase_full_pi = center_phase_observed_pi - center_phase_prev_pi - phase_applied_ps_pi;
		// calculate the smallest additional phase (assume well behaved lasers)
		float add_phase_mod_pi = add_phase_full_pi - hls::round(add_phase_full_pi/2.)*2.;
		// calculate the full phase correction per time unit (phase slope)
		full_phase_slope_pi = (phase_applied_ps_pi + add_phase_mod_pi) / delta_time;
	}
	// unwrap using change of the phase change
	if(1){
		// calculate the leftover phase at the next ifg and correct
		// first, predict phase correction for the entire ifg-ifg distance
		float phase_applied_ps_pi = phase_slope_pi * delta_time;
		// float phase_applied_ps_mod_pi = remainder(phase_applied_ps_pi , 2.);
		float phase_applied_ps_mod_pi = phase_applied_ps_pi - float(int(phase_applied_ps_pi/2.)*2);

		// how much additional phase do we need?
		phase_change_pi = center_phase_observed_pi - center_phase_prev_pi - phase_applied_ps_mod_pi;
		phase_change_pi = (phase_change_pi - phase_change_prev_pi) > 1. ? phase_change_pi - 2. : phase_change_pi;
		phase_change_pi = (phase_change_pi - phase_change_prev_pi) > 1. ? phase_change_pi - 2. : phase_change_pi;
		phase_change_pi = (phase_change_pi - phase_change_prev_pi) > 1. ? phase_change_pi - 2. : phase_change_pi;

		phase_change_pi = (phase_change_pi - phase_change_prev_pi) < -1. ? phase_change_pi + 2. : phase_change_pi;
		phase_change_pi = (phase_change_pi - phase_change_prev_pi) < -1. ? phase_change_pi + 2. : phase_change_pi;
		phase_change_pi = (phase_change_pi - phase_change_prev_pi) < -1. ? phase_change_pi + 2. : phase_change_pi;
		// calculate the full phase correction per time unit (phase slope)
		full_phase_slope_pi = (phase_applied_ps_pi + phase_change_pi) / delta_time;
	}

	// calculate the sampling time interval to correct for repetition rate changes
	// start with slight downsampling to be able to correct in both directions
	float sampling_time_unit = inv95;
	if(offset_time0 > 0)
		sampling_time_unit = delta_time_exact / offset_time0 * inv95; // * t_unit

	// put all correction data in structure and send
	// first one is ignored by process worker because cd.center_time_prev == 0
	correction_data_type cd;
	cd.center_phase_prev_pi = center_phase_prev_pi;
	cd.phase_slope_pi = full_phase_slope_pi;
	cd.center_time_prev = center_time_prev;
	//cd.center_time_prev_exact = center_time_prev_exact;
	cd.center_time_observed = center_time_observed;
	//cd.center_time_observed_exact = center_time_observed_exact;

	cd.sampling_time_unit = fp(sampling_time_unit);
	cd.start_sending_time = fp_time(center_time_observed_exact - fp_long(num_samples/2)*fp_long(sampling_time_unit));

	cd.retain_samples = num_samples;
	cd.phase_mult = 1;

	correction_data_type cd2;
	cd2.center_phase_prev_pi = center_phase_prev_pi;
	cd2.phase_slope_pi = full_phase_slope_pi;
	cd2.center_time_prev = center_time_prev;
	//cd2.center_time_prev_exact = center_time_prev_exact;
	cd2.center_time_observed = center_time_observed;
	//cd2.center_time_observed_exact = center_time_observed_exact;

	cd2.sampling_time_unit = fp(sampling_time_unit);
	cd2.start_sending_time = fp_time(center_time_observed_exact - fp_long(num_samples/2)*fp_long(sampling_time_unit)) + delay_ch_2;

	cd2.retain_samples = num_samples;
	cd2.phase_mult = phase_mult_ch_2;

	// do not send correction data for the first ifg as it is incomplete
	if(cnt_proc_loops > 0){
		out_correction_data_q.write(cd);
		out_correction_data_ch_2_q.write(cd2);

		log_data_type ld;
		ld.delta_time_exact = delta_time_exact;
		ld.phase = center_phase_observed_pi;
		ld.center_freq = center_freq_observed;
		ld.phase_change_pi = phase_change_pi;

		out_log[stage_log] = ld;
		if (stage_log==3)
			out_log_data_q.write(out_log);
		stage_log++;
	}
	// retain data for next iteration
	//center_time_prev = center_time_observed;
	center_time_prev = center_time_observed;
	center_time_prev_exact = center_time_observed_exact;
	center_phase_prev_pi = center_phase_observed_pi;
	phase_change_prev_pi = phase_change_pi;
	cnt_proc_loops = cnt_proc_loops + 1;
}
