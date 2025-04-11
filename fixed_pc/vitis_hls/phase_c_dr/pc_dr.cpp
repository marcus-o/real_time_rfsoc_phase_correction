#include "hls_print.h"
#include "pc_dr.h"

// hilbert worker sends 50 zeros before it starts sending the signal
void hilbert_worker(
		hls::stream<adc_data_two_val> &in_sig_q,
		hls::stream<adc_data_compl, 100> &out_sig_h_q){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE axis register port = in_sig_q depth=2
	#pragma HLS INTERFACE ap_fifo register port = out_sig_h_q depth=100
	#pragma HLS PIPELINE II=1

	const int buf_size0 = 51;
	const int hilbert_mid_adj = 25;

	static adc_data_two_val inc_buf[buf_size0];
	#pragma HLS array_partition variable=inc_buf type=complete
	#pragma HLS reset variable=inc_buf off

	// used to multiply with fmax/2: cnt0*2, cnt1*0, cnt2*-2, cnt3*0
	static single_digit swap = 2;
	#pragma HLS reset variable=swap

	// shift by one
	for(int cnt=0; cnt<buf_size0-1; cnt++){
		#pragma hls unroll
		inc_buf[cnt] = inc_buf[cnt+1];
	}
	// fill last position
	inc_buf[buf_size0-1] = in_sig_q.read();;

	adc_data mid_val = inc_buf[hilbert_mid_adj].v1;
	fp sum_temp = 0;
	for(int cnt=0; cnt<buf_size0; cnt++){
		#pragma hls unroll
		sum_temp -= fp(inc_buf[cnt].v2) * fir_coeffs_hilbert[cnt*2+1];
	}
	out_sig_h_q.write(adc_data_compl(mid_val*swap, adc_data(sum_temp)*swap));
	swap = -1*swap;
}

fp abs_sq(fp_compl_short a){
	#pragma HLS inline
	return a.real()*a.real() + a.imag()*a.imag();
	}

fp_long abs_sq(fp_compl a){
	#pragma HLS inline
	return a.real()*a.real() + a.imag()*a.imag();
	}

int abs_sq(adc_data_compl a){
	#pragma HLS inline
	return int(a.real())*int(a.real()) + int(a.imag())*int(a.imag());
	}

void trig_worker(
		hls::stream<adc_data_compl, 100> &in_sig_h_q,
		hls::stream<adc_data_compl, wait_buf_size> &out_proc_q,
		hls::stream<fp_compl, 100> &out_ifg_q,
		hls::stream<time_int, 100> &out_ifg_time_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE ap_fifo register port = in_sig_h_q depth=100
	#pragma HLS INTERFACE ap_fifo register port = out_proc_q depth=wait_buf_size
	#pragma HLS INTERFACE ap_fifo register port = out_ifg_q depth=100
	#pragma HLS INTERFACE ap_fifo register port = out_ifg_time_q depth=100
	#pragma HLS PIPELINE II=1

	const int trig_val_sq = trig_val*trig_val;
	const int buf_size1 = size_ifg_2 + size_spec_2;

	static hls::stream<adc_data_compl, buf_size1+10> inc_buf;
	#pragma HLS reset variable=inc_buf off

	static time_int time_current = 0;
	#pragma HLS reset variable=time_current
	static int remaining_packets = 0;
	#pragma HLS reset variable=remaining_packets
	static int prev_write_val_abs_sq = 0;
	#pragma HLS reset variable=prev_write_val_abs_sq
	static int loop_cnt = 0;
	#pragma HLS reset variable=loop_cnt

	// all reads and writes
	adc_data_compl cur_write_val = in_sig_h_q.read();
	inc_buf.write(cur_write_val);
	int cur_write_val_abs_sq = abs_sq(cur_write_val);

	if(loop_cnt < buf_size1){
		loop_cnt++;
	} else {
		// pass on the data
		adc_data_compl cur_read_val = inc_buf.read();
		out_proc_q.write(cur_read_val);

		if(!remaining_packets){
			if (
					(time_current > buf_size1)
					&& (cur_write_val_abs_sq > trig_val_sq)
					&& (prev_write_val_abs_sq < trig_val_sq)){
				remaining_packets = 2*buf_size1;
			}
		}else{
			out_ifg_q.write(fp_compl(fp(cur_read_val.real()), fp(cur_read_val.imag())));
			out_ifg_time_q.write(time_current);
			remaining_packets -= 1;
		}
		time_current = time_current + 1; // t_unit;
	}
	// save current value for next iteration
	prev_write_val_abs_sq = cur_write_val_abs_sq;
}

void measure_worker(
		hls::stream<fp_compl, 100> &in_ifg_q,
		hls::stream<time_int, 100> &in_ifg_time_q,
		hls::stream<correction_data_type, 5> &out_correction_data_q,
		hls::stream<log_data_packet> &out_log_data_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE ap_fifo register port = in_ifg_q depth=100
	#pragma HLS INTERFACE ap_fifo register port = in_ifg_time_q depth=100
	#pragma HLS INTERFACE ap_fifo register port = out_correction_data_q depth=5
	#pragma HLS INTERFACE axis register port = out_log_data_q depth=5

	const int min_f_idx = 0;
	const int max_f_idx = size_spec_2;
	const int buf_size2 = 2*(size_ifg_2 + size_spec_2);

	static time_int center_time_prev = 0;
	#pragma HLS reset variable=center_time_prev
	static fp_time center_time_prev_exact = 0;
	#pragma HLS reset variable=center_time_prev_exact

	static float offset_time0 = 0;
	#pragma HLS reset variable=offset_time0
	static float center_phase_prev_pi = 0;
	#pragma HLS reset variable=center_phase_prev_pi
	static float center_freq0 = 0;
	#pragma HLS reset variable=center_freq0

	static int cnt_proc_loops = 0;
	#pragma HLS reset variable=cnt_proc_loops
	static int ld_write_cnt = 0;
	#pragma HLS reset variable=ld_write_cnt


	fp_compl ifg_in1[buf_size2];
	time_int t_in1[buf_size2];
	// receive
	for(int idx=0; idx<buf_size2; idx++){
		#pragma HLS pipeline II=1 style=frp
		ifg_in1[idx] = in_ifg_q.read();
		t_in1[idx] = in_ifg_time_q.read();
	}

	// measure time
	fp_long proc1_sum1 = 0;
	fp_long proc1_sum2 = 0;
	for(int idx=size_spec_2; idx<size_spec_2 + 2*size_ifg_2; idx++){
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
	fp_compl_short fft_in_buf[2*size_spec_2]; // careful about the datatype
	fp_compl_short spec_ifg_buf[2*size_spec_2];
	for(int idx=0; idx<2*size_spec_2; idx++){
		#pragma HLS pipeline II=1
		fp_compl temp = ifg_in1[idx + shift]*fp_compl(fp(1/65536.), 0.);
		fft_in_buf[idx] = fp_compl_short(fp_short(temp.real()), fp_short(temp.imag()));
	}
	config_t fft_config1;
	status_t fft_status1;
	fft_config1.setDir(1);
	hls::fft<param1>(fft_in_buf, spec_ifg_buf, &fft_status1, &fft_config1);
	fp_compl_short spec_ifg[2*size_spec_2];
	for (int idx=0; idx<2*size_spec_2; idx++){
		#pragma HLS pipeline II=1
		spec_ifg[idx] = spec_ifg_buf[idx];
	}

	// measure central frequency
	// must treat both pos and neg frequencies
	fp_long proc2_sum1 = 0;
	fp_long proc2_sum2 = 0;
	for (int idx=min_f_idx; idx<max_f_idx; idx++){
		#pragma HLS pipeline II=1
		fp abs_spec_ifg_temp = abs_sq(spec_ifg[idx]); // does not have to be scaled as it is scaled before fft /fp_long(65536);
		proc2_sum1 += fp_long(abs_spec_ifg_temp * fp(idx));
		proc2_sum2 += fp_long(abs_spec_ifg_temp);
	}
	for (int idx=max_f_idx; idx<2*max_f_idx; idx++){
		#pragma HLS pipeline II=1
		fp abs_spec_ifg_temp = abs_sq(spec_ifg[idx]); // does not have to be scaled as it is scaled before fft /fp_long(65536);
		proc2_sum1 += fp_long(abs_spec_ifg_temp * fp(idx - 2*max_f_idx));
		proc2_sum2 += fp_long(abs_spec_ifg_temp);
	}
	float center_freq_observed = 0;
	if(proc2_sum2 > fp_long(0))
		center_freq_observed = proc2_sum1.to_float() / proc2_sum2.to_float() / float(size_spec_2*2); // / fp_small_long(t_unit);

	// first interferogram defines the frequency for all of them
	if (cnt_proc_loops == 0)
		center_freq0 = center_freq_observed;

	// first and second interferogram define the sampling rate
	//float delta_time = (center_time_observed - center_time_prev).to_float();
	float delta_time = (center_time_observed - center_time_prev).to_float();
	float delta_time_exact = (center_time_observed_exact - center_time_prev_exact).to_float();

	if(cnt_proc_loops == 1)
		offset_time0 = delta_time;

	// slope of the phase needed to shift frequency to zero
	float phase_slope_pi = center_freq0 * 2.; //* fp_small_long(t_unit)

	// calculate the leftover phase at the next ifg and correct
	// first, predict phase correction for the entire ifg-ifg distance
	float phase_applied_ps_pi = phase_slope_pi * delta_time;
	// how much additional phase do we need?
	float add_phase_full_pi = center_phase_observed_pi - center_phase_prev_pi - phase_applied_ps_pi;
	// calculate the smallest additional phase (assume well behaved lasers)
	float add_phase_mod_pi = add_phase_full_pi - hls::round(add_phase_full_pi/2)*2;
	// calculate the full phase correction per time unit (phase slope)
	float full_phase_slope_pi = (phase_applied_ps_pi + add_phase_mod_pi) / delta_time;

	// calculate the sampling time interval to correct for repetition rate changes
	// start with slight downsampling to be able to correct in both directions
	float sampling_time_unit = inv95;
	if(offset_time0 > 0)
		sampling_time_unit = delta_time / offset_time0 * inv95; // * t_unit

	// put all correction data in structure and send
	// first one is ignored by process worker because cd.center_time_prev == 0
	correction_data_type cd;
	cd.center_phase_prev_pi = center_phase_prev_pi;
	cd.phase_slope_pi = full_phase_slope_pi;
	cd.center_time_prev = center_time_prev;
	cd.center_time_prev_exact = center_time_prev;
	cd.center_time_observed = center_time_observed;
	cd.center_time_observed_exact = center_time_observed_exact;
	cd.sampling_time_unit = fp(sampling_time_unit);
	cd.start_sending_time = fp_time(center_time_observed_exact - fp_long(retain_samples/2)*fp_long(sampling_time_unit));

	// do not send correction data for the first ifg as it is incomplete
	if(cnt_proc_loops > 0){
		out_correction_data_q.write(cd);

		log_data_packet ld;
		ld.data.delta_time = delta_time_exact;
		ld.data.phase = center_phase_observed_pi;
		ld.data.center_freq = center_freq_observed;
		ld.keep = -1;
		ld.strb = -1;
		ld.last = (ld_write_cnt == (ld_packets-1));
		out_log_data_q.write(ld);
		ld_write_cnt = (ld_write_cnt < (ld_packets-1)) ? ld_write_cnt+1 : 0;
	}
	// retain data for next iteration
	//center_time_prev = center_time_observed;
	center_time_prev = center_time_observed;
	center_time_prev_exact = center_time_observed_exact;
	center_phase_prev_pi = center_phase_observed_pi;
	cnt_proc_loops = cnt_proc_loops + 1;
}

void process_worker_primer(
		hls::stream<adc_data_compl, wait_buf_size> &in_q,
		hls::stream<correction_data_type, 5> &in_correction_data_q,
		hls::stream<process_data_type, 10> &out_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE ap_fifo register port = in_q depth=wait_buf_size
	#pragma HLS INTERFACE ap_fifo register port = in_correction_data_q depth=5
	#pragma HLS INTERFACE ap_fifo register port = out_q depth=2
	#pragma HLS PIPELINE II=1

	static time_int time_current = 0;
	#pragma HLS reset variable=time_current
	static correction_data_type cd;
	#pragma HLS reset variable=cd

	// starts after config data of second interferogram is received
	// check each iteration if it needs new correction data
	// (time_current has progressed past the maximum valid time of the current correction data)
	if(time_current >= cd.center_time_observed)
		if(!in_correction_data_q.read_nb(cd))
			return;
	process_data_type out;
	out.val = in_q.read();
	out.time_current = time_current;
	out.correction_data = cd;
	out_q.write(out);
	time_current = time_current + 1; //+t_unit;
}

void process_worker(
		hls::stream<process_data_type, 10> &in_q,
		hls::stream<adc_data_compl_4sampl, 100> &out_q,
		hls::stream<adc_data_compl_4sampl_packet> &out_orig_q,
		hls::stream<adc_data_compl_4sampl_packet> &out_orig_corrected_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE ap_fifo register port = in_q depth=2
	#pragma HLS INTERFACE ap_fifo register port = out_q depth=100
	#pragma HLS INTERFACE axis register port = out_orig_q depth=2
	#pragma HLS INTERFACE axis register port = out_orig_corrected_q depth=2
	#pragma HLS PIPELINE II=1

	static fp_time sampling_time_current = 0;
	#pragma HLS reset variable=sampling_time_current
	static fp_time sampling_time_current2 = 0;
	#pragma HLS reset variable=sampling_time_current2

	static fp_compl prev_inc_corr = fp_compl(0, 0);
	#pragma HLS reset variable=prev_inc_corr

	static int send_packets = retain_samples;
	#pragma HLS reset variable=retain_samples
	static int orig_sent_samples = orig_retain_samples;
	#pragma HLS reset variable=orig_sent_samples
	static int orig_corrected_sent_samples = orig_retain_samples;
	#pragma HLS reset variable=orig_corrected_sent_samples

	static adc_data_compl_4sampl out;
	#pragma HLS reset variable=out off
	static adc_data_compl_4sampl out_orig;
	#pragma HLS reset variable=out_orig off
	static adc_data_compl_4sampl out_orig_corrected;
	#pragma HLS reset variable=out_orig_corrected off

	static int stage = 0;
	#pragma HLS reset variable=stage
	static int orig_stage = 0;
	#pragma HLS reset variable=orig_stage
	static int orig_corrected_stage = 0;
	#pragma HLS reset variable=orig_corrected_stage

	static bool first = true;
	#pragma HLS reset variable=first

	process_data_type in = in_q.read();

	adc_data_compl in_val = in.val;
	time_int time_current = in.time_current;
	correction_data_type cd = in.correction_data;

	if(first){
		sampling_time_current = cd.start_sending_time;
		sampling_time_current2 = cd.start_sending_time;
		first = false;
	}

	//correct and send data
	float phase1_pi = cd.center_phase_prev_pi + cd.phase_slope_pi * (time_current - cd.center_time_prev).to_float();
	fp phase_mod_pi = phase1_pi - hls::floor(phase1_pi/2)*2;
	fp_compl correction = fp_compl(hls::cospi(-phase_mod_pi), hls::sinpi(-phase_mod_pi));
	fp_compl inc_corr = fp_compl(fp(in_val.real()), fp(in_val.imag())) * correction;

	// check if the down sampled signal has a data point between the last two samples
	if(time_current >= sampling_time_current)
	{
		// if yes, interpolate
		fp time_left = fp(sampling_time_current - (time_current - 1)); //-t_unit);
		fp_compl interp = prev_inc_corr + (inc_corr - prev_inc_corr) * time_left; // * t_unit_inv;

		// if the last packet of the current interferogram was sent, load the starting point of the next interferogram
		// otherwise, use the current sampling time unit to calculate the next sampling time point.
		// this will change at each interferogram peak to the new value
		sampling_time_current = (send_packets==1) ? cd.start_sending_time : fp_time(sampling_time_current + cd.sampling_time_unit);
		send_packets = (send_packets==1) ? retain_samples : send_packets - 1;

		// pack 4 values
		adc_data_compl val = adc_data_compl(adc_data(interp.real()), adc_data(interp.imag()));
		if (stage==0) out.v1 = val;
		else if (stage==1) out.v2 = val;
		else if (stage==2) out.v3 = val;
		else if (stage==3){
			out.v4 = val;
			out_q.write(out);
		}
		stage = (stage==3) ? 0 : stage + 1;
	}

	// used to send unaveraged data (full data stream)
	// corrected data, same logic as above
	if(time_current >= sampling_time_current2)
	{
		fp time_left = sampling_time_current2 - (time_current - 1); //-t_unit);
		fp_compl interp = prev_inc_corr + (inc_corr - prev_inc_corr) * time_left; // * t_unit_inv;
		sampling_time_current2 = sampling_time_current2 + cd.sampling_time_unit;

		// pack 4 values
		bool last = (orig_corrected_sent_samples == 1);
		adc_data_compl val = adc_data_compl(adc_data(interp.real()), adc_data(interp.imag()));
		if (orig_corrected_stage==0) out_orig_corrected.v1 = val;
		else if (orig_corrected_stage==1) out_orig_corrected.v2 = val;
		else if (orig_corrected_stage==2) out_orig_corrected.v3 = val;
		else if (orig_corrected_stage==3){
			adc_data_compl_4sampl_packet out1;
			out_orig_corrected.v4 = val;
			out1.data = out_orig_corrected;
			out1.keep = -1;
			out1.strb = -1;
			out1.last = last ? 1 : 0;
			out_orig_corrected_q.write(out1);
		}
		orig_corrected_stage = (orig_corrected_stage==3) ? 0 : orig_corrected_stage + 1;
		orig_corrected_sent_samples = last ? orig_retain_samples : orig_corrected_sent_samples - 1;
	}

	// used to send unaveraged data (full data stream)
	// uncorrected data
	{
		// pack 4 values
		adc_data_compl val = adc_data_compl(adc_data(in_val.real()), adc_data(in_val.imag()));
		bool last = (orig_sent_samples == 1);
		if (orig_stage==0) out_orig.v1 = val;
		else if (orig_stage==1) out_orig.v2 = val;
		else if (orig_stage==2) out_orig.v3 = val;
		else if(orig_stage==3){
			adc_data_compl_4sampl_packet out1;
			out_orig.v4 = val;
			out1.data = out_orig;
			out1.keep = -1;
			out1.strb = -1;
			out1.last = last ? 1 : 0;
			out_orig_q.write(out1);
		}
		orig_stage = (orig_stage==3) ? 0 : orig_stage + 1;
		orig_sent_samples = last ? orig_retain_samples : orig_sent_samples - 1;
	}
	// data for the next iteration
	prev_inc_corr = inc_corr;
}

void avg_worker(
		hls::stream<adc_data_compl_4sampl, 100> &in_q,
		hls::stream<adc_data_double_length_compl_2sampl_packet> &out_q,
		hls::stream<ap_int<32>, 10> &sig_num_avgs_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE ap_fifo register port=in_q depth=100
	#pragma HLS INTERFACE axis register port=out_q depth=2
	#pragma HLS INTERFACE ap_fifo register port=sig_num_avgs_q depth=2

	static adc_data_double_length_compl_4sampl buf[retain_samples/4];
	#pragma HLS bind_storage variable=buf type=RAM_2P impl=uram
	#pragma HLS reset variable=buf off

	// average
	static ap_int<32> avgs = 5;
	#pragma HLS reset variable=avgs

	for(int cnt1=0; cnt1<retain_samples/4; cnt1++)
	{
		#pragma HLS PIPELINE II=2
		adc_data_compl_4sampl in1 = in_q.read();
		adc_data_double_length_compl_4sampl in2;
		in2.v1 = in1.v1;
		in2.v2 = in1.v2;
		in2.v3 = in1.v3;
		in2.v4 = in1.v4;
		buf[cnt1] = in2;
		sig_num_avgs_q.read_nb(avgs);
	}

	for(int cnt2=1; cnt2<avgs-1; cnt2++)
	{
		for(int cnt1=0; cnt1<retain_samples/4; cnt1++)
		{
			#pragma HLS PIPELINE II=2
			adc_data_compl_4sampl in1 = in_q.read();
			adc_data_double_length_compl_4sampl in2 = buf[cnt1];
			in2.v1 += in1.v1;
			in2.v2 += in1.v2;
			in2.v3 += in1.v3;
			in2.v4 += in1.v4;
			buf[cnt1] = in2;
		}
	}

	for(int cnt1=0; cnt1<retain_samples/4; cnt1++)
	{
		#pragma HLS PIPELINE II=2
		adc_data_compl_4sampl in1 = in_q.read();
		adc_data_double_length_compl_4sampl in2 = buf[cnt1];

		adc_data_double_length_compl_2sampl_packet out1;
		out1.data.v1 = adc_data_double_length_compl(in2.v1.real() + in1.v1.real(), in2.v1.imag() + in1.v1.imag());
		out1.data.v2 = adc_data_double_length_compl(in2.v2.real() + in1.v2.real(), in2.v2.imag() + in1.v2.imag());
		out1.keep = -1;
		out1.strb = -1;
		out1.last = 0;
		out_q.write(out1);

		adc_data_double_length_compl_2sampl_packet out2;
		out2.data.v1 = adc_data_double_length_compl(in2.v3.real() + in1.v3.real(), in2.v3.imag() + in1.v3.imag());
		out2.data.v2 = adc_data_double_length_compl(in2.v4.real() + in1.v4.real(), in2.v4.imag() + in1.v4.imag());
		out2.keep = -1;
		out2.strb = -1;
		out2.last = (cnt1==retain_samples/4-1) ? 1 : 0;
		out_q.write(out2);
	}
}

void in_avgs_worker(
		hls::stream<ap_int<32>> &num_avgs_q,
		hls::stream<ap_int<32>, 10> &sig_num_avgs_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE axis register port=num_avgs_q
	#pragma HLS INTERFACE ap_fifo register port=sig_num_avgs_q
	#pragma HLS PIPELINE II=1

	sig_num_avgs_q.write(num_avgs_q.read());
}

// main function
void pc_dr(
		hls::stream<adc_data_two_val> &in_q,
		hls::stream<adc_data_double_length_compl_2sampl_packet> &out_q,
		hls::stream<ap_int<32>> &num_avgs_q,
		hls::stream<log_data_packet> &out_log_data_q,
		hls::stream<adc_data_compl_4sampl_packet> &out_orig_q,
		hls::stream<adc_data_compl_4sampl_packet> &out_orig_corrected_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE axis register port = in_q depth=2
	#pragma HLS INTERFACE axis register port = out_q depth=2
	#pragma HLS INTERFACE axis register port = num_avgs_q depth=2
	#pragma HLS INTERFACE axis register port = out_log_data_q depth=2
	#pragma HLS INTERFACE axis register port = out_orig_q depth=2
	#pragma HLS INTERFACE axis register port = out_orig_corrected_q depth=2

	// launch all parallel tasks and their communication
	hls_thread_local hls::stream<adc_data_compl, 100> sig_h_q;
	hls_thread_local hls::task t1(hilbert_worker, in_q, sig_h_q);

	hls_thread_local hls::stream<adc_data_compl, wait_buf_size> proc1_buf_out_q;
	hls_thread_local hls::stream<fp_compl, 100> ifg1_q;
	hls_thread_local hls::stream<time_int, 100> ifg1_time_q;
	hls_thread_local hls::task t2(trig_worker, sig_h_q, proc1_buf_out_q, ifg1_q, ifg1_time_q);

	hls_thread_local hls::stream<correction_data_type, 5> correction_data1_q;
	hls_thread_local hls::task t3(measure_worker, ifg1_q, ifg1_time_q, correction_data1_q, out_log_data_q);

	hls_thread_local hls::stream<process_data_type, 10> proc1_out_q;
	hls_thread_local hls::task t4(process_worker_primer, proc1_buf_out_q, correction_data1_q, proc1_out_q);

	hls_thread_local hls::stream<adc_data_compl_4sampl, 100> avg_q;
	hls_thread_local hls::task t4b(process_worker, proc1_out_q, avg_q, out_orig_q, out_orig_corrected_q);

	hls_thread_local hls::stream<ap_int<32>, 10> sig_num_avgs_q;
	hls_thread_local hls::task t5(in_avgs_worker, num_avgs_q, sig_num_avgs_q);

	hls_thread_local hls::task t6(avg_worker, avg_q, out_q, sig_num_avgs_q);

}

