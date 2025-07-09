#include "pc_dr.h"
#include "hls_cordic_apfixed.h"

void process_worker_primer(
		hls::stream<adc_data_compl_vec> &in_q,
		hls::stream<correction_data_type, 5> &in_correction_data_q,
		hls::stream<process_data_type, 2> &out_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	static time_int time_current = 0;
	#pragma HLS reset variable=time_current
	static correction_data_type cd;
	#pragma HLS reset variable=cd
	static correction_data_type cd_next;
	#pragma HLS reset variable=cd_next

	// starts after config data of second interferogram is received
	// check each iteration if it needs new correction data
	// (time_current has progressed past the maximum valid time of the current correction data)

	if((time_current + time_int(3*samples_per_clock)) - cd_next.center_time_observed >= 0){
		//if(!in_correction_data_q.read_nb(cd_next))
		//	return;
		cd_next = in_correction_data_q.read();
	}

	cd = time_current - cd.center_time_observed >= 0 ? cd_next : cd;

	adc_data_compl_vec in = in_q.read();
	process_data_type out;
	out.correction_data = cd;
	out.correction_data_next = cd_next;
	for (char cnt=0; cnt<samples_per_clock; cnt++){
		out.signal_vec[cnt] = in[cnt];
		out.next_correction[cnt] = (time_current+time_int(cnt)) >= cd.center_time_observed ? true : false;
	}

	for(char cnt=0; cnt<samples_per_clock+1; cnt++){
	#pragma hls unroll
		// this switches the resampling grid only every 8 samples (shouldn't make a difference as the change is small)
		out.cnt_x_times[cnt] = ap_uint<5>(cnt)*cd.sampling_time_unit - (samples_per_clock-1) - time_current;
	}

	out_q.write(out);
	time_current = time_current + samples_per_clock;
}

void process_worker(
		hls::stream<process_data_type, 2> &in_q,
		hls::stream<process_data_type_reduced, 2> &avg_interp_q,
		hls::stream<process_data_type_reduced, 2> &orig_corrected_interp_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	static fp_compl prev_inc_corr = fp_compl(0, 0);
	#pragma HLS reset variable=prev_inc_corr

	static time_int time_current = 0;
	#pragma HLS reset variable=time_current

	process_data_type in = in_q.read();

	// correct all current samples at once
	fp_compl_vec inc_corr_vec;
	correct: {
		for(char cnt=0; cnt<samples_per_clock; cnt++){
			#pragma HLS UNROLL
			//correct and send data
			correction_data_type cd_loc = in.next_correction[cnt] ? in.correction_data_next : in.correction_data;
			float phase1_pi = cd_loc.center_phase_prev_pi + cd_loc.phase_slope_pi * (time_current + time_int(cnt) - cd_loc.center_time_prev).to_float();

			// float has 24 bits of precision, this mod moves these in a well defined range
			//ap_fixed<24, 5> phase_mod_pi = ap_fixed<24,5>(-1*(phase1_pi - hls::floor(phase1_pi/2)*2));
			ap_fixed<24, 2> phase_mod_pi = ap_fixed<24,2>(-1*(phase1_pi - int(phase1_pi/2)*2));

			ap_fixed<24, 2> corr_imag;
			ap_fixed<24, 2> corr_real;
			cordic_apfixed::generic_sincospi(phase_mod_pi, corr_imag, corr_real);

			inc_corr_vec[cnt] = fp_compl(
					in.signal_vec[cnt].real()*corr_real - in.signal_vec[cnt].imag()*corr_imag,
					in.signal_vec[cnt].real()*corr_imag + in.signal_vec[cnt].imag()*corr_real);
		}
	}

	process_data_type_reduced out;
	out.signal_vec = inc_corr_vec;
	out.cnt_x_times = in.cnt_x_times;
	out.start_sending_time = in.correction_data.start_sending_time;
	out.retain_samples = in.correction_data.retain_samples;
	out.prev_inc_corr = prev_inc_corr;
	avg_interp_q.write(out);
	orig_corrected_interp_q.write(out);

	// save last corrected sample for the next iteration interpolation
	prev_inc_corr = inc_corr_vec[samples_per_clock-1];
	time_current = time_current + samples_per_clock;
}

void avg_interp(
		hls::stream<process_data_type_reduced, 2> &in_q,
		hls::stream<adc_data_compl_vec_info> &avg_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	static fp_time sampling_time_current = 0;
	#pragma HLS reset variable=sampling_time_current

	static int send_packets = 0;
	#pragma HLS reset variable = send_packets

	static time_int time_current = 0;
	#pragma HLS reset variable=time_current

	process_data_type_reduced in = in_q.read();

	if(send_packets <= 0){
		// if the last packet of the current interferogram was sent, load the starting point of the next interferogram
		// this tosses samples_per_clock samples but that doesnt matter as these are tossed by the resampling anyways
		sampling_time_current = in.start_sending_time;
		send_packets = in.retain_samples;
	}else{

		char no_samples = 0;
		hls::vector<fp_time, samples_per_clock> next_sampling_time;
		fp_time sampling_time_current_temp = sampling_time_current;
		int send_packets_temp = send_packets;

		for(char cnt=0; cnt<samples_per_clock; cnt++){
			#pragma HLS UNROLL
			if (sampling_time_current_temp + in.cnt_x_times[cnt] < 0){
				sampling_time_current = sampling_time_current_temp + in.cnt_x_times[cnt+1] + samples_per_clock-1 + time_current;
				no_samples = cnt+1;
				send_packets = send_packets_temp - (cnt+1);
			}
		}

		for(char cnt=0; cnt<samples_per_clock; cnt++){
			#pragma HLS UNROLL
			next_sampling_time[cnt] = sampling_time_current_temp + in.cnt_x_times[cnt] + samples_per_clock-1 + time_current;
		}

		adc_data_compl_vec interp_vec;
		for(char cnt=0; cnt<samples_per_clock; cnt++){
			#pragma HLS UNROLL

			if(next_sampling_time[cnt] < fp_time(time_current + time_int(samples_per_clock-1))){
				fp idx_right_temp = next_sampling_time[cnt] - fp_time(time_current);
				char idx_right = idx_right_temp>0 ? char(idx_right_temp.to_int()+1) : char(0);
				fp time_left = fp(next_sampling_time[cnt] - fp_time(time_current + time_int(idx_right) - time_int(1)));
				fp_compl prev_inc_corr_loc = idx_right ? in.signal_vec[idx_right-1] : in.prev_inc_corr;
				fp_compl interp = prev_inc_corr_loc + (in.signal_vec[idx_right] - prev_inc_corr_loc) * time_left;
				interp_vec[cnt] = adc_data_compl(adc_data(interp.real()), adc_data(interp.imag()));
			}
		}
		if(no_samples>0){
			adc_data_compl_vec_info out;
			out.data = interp_vec;
			out.no_samples = no_samples >= send_packets_temp ? send_packets_temp : no_samples;
			avg_q.write(out);
		}
	}
	time_current = time_current + samples_per_clock;
}

void orig_corrected_interp(
		hls::stream<process_data_type_reduced, 2> &in_q,
		hls::stream<adc_data_compl_vec_info> &out_orig_corrected_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	static fp_time sampling_time_current = 0;
	#pragma HLS reset variable=sampling_time_current

	static bool first = true;
	#pragma HLS reset variable=first

	static time_int time_current = 0;
	#pragma HLS reset variable=time_current

	process_data_type_reduced in = in_q.read();

	if(first){
		sampling_time_current = in.start_sending_time;
		first = false;
	}else{

		char no_samples = 0;
		hls::vector<fp_time, samples_per_clock> next_sampling_time;
		fp_time sampling_time_current_temp = sampling_time_current;
		for(char cnt=0; cnt<samples_per_clock; cnt++){
			#pragma HLS UNROLL
			if (sampling_time_current_temp + in.cnt_x_times[cnt] < 0){
				sampling_time_current = sampling_time_current_temp + in.cnt_x_times[cnt+1] + samples_per_clock-1 + time_current;
				no_samples = cnt+1;
			}
		}

		for(char cnt=0; cnt<samples_per_clock; cnt++){
			#pragma HLS UNROLL
			next_sampling_time[cnt] = sampling_time_current_temp + in.cnt_x_times[cnt] + samples_per_clock-1 + time_current;
		}
		// interpolate on new time axis
		adc_data_compl_vec interp_vec;
		for(char cnt=0; cnt<samples_per_clock; cnt++){
			#pragma HLS UNROLL

			if(next_sampling_time[cnt] < fp_time(time_current + time_int(samples_per_clock-1))){
				fp idx_right_temp = next_sampling_time[cnt] - fp_time(time_current);
				char idx_right = idx_right_temp>0 ? char(idx_right_temp.to_int()+1) : char(0);
				fp time_left = fp(next_sampling_time[cnt] - fp_time(time_current + time_int(idx_right) - time_int(1)));
				fp_compl prev_inc_corr_loc = idx_right ? in.signal_vec[idx_right-1] : in.prev_inc_corr;
				fp_compl interp = prev_inc_corr_loc + (in.signal_vec[idx_right] - prev_inc_corr_loc) * time_left;
				interp_vec[cnt] = adc_data_compl(adc_data(interp.real()), adc_data(interp.imag()));
			}
		}

		// send on
		if(no_samples>0){
			adc_data_compl_vec_info out;
			out.data = interp_vec;
			out.no_samples = no_samples;
			out_orig_corrected_q.write(out);
		}
	}
	time_current = time_current + samples_per_clock;
}

void in_fifo_correction(
	hls::stream<correction_data_type> &in_q,
	hls::stream<correction_data_type, 5> &out_q
	){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	out_q.write(in_q.read());
}

// main function
void pc_dr(
		hls::stream<adc_data_compl_vec> &proc1_buf_in_q,
		hls::stream<adc_data_compl_vec_info> &out_orig_corrected_q,
		hls::stream<adc_data_compl_vec_info> &avg_q,
		hls::stream<correction_data_type> &in_correction_data1_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE axis register port = proc1_buf_in_q
	#pragma HLS INTERFACE axis register port = out_orig_corrected_q
	#pragma HLS INTERFACE axis register port = avg_q
	#pragma HLS INTERFACE axis register port = in_correction_data1_q

	// launch all parallel tasks and their communication
	hls_thread_local hls::stream<correction_data_type, 5> correction_data_fifo_q;
	hls_thread_local hls::task t1(
			in_fifo_correction, in_correction_data1_q, correction_data_fifo_q);
	hls_thread_local hls::stream<process_data_type, 2> proc1_out_q;
	hls_thread_local hls::task t2(
			process_worker_primer, proc1_buf_in_q, correction_data_fifo_q, proc1_out_q);

	hls_thread_local hls::stream<process_data_type_reduced, 2> avg_interp_q;
	hls_thread_local hls::stream<process_data_type_reduced, 2> orig_corrected_interp_q;
	hls_thread_local hls::task t3(
		process_worker,
		proc1_out_q,
		avg_interp_q,
		orig_corrected_interp_q
		);
	hls_thread_local hls::task t4(
		avg_interp,
		avg_interp_q,
		avg_q
		);
	hls_thread_local hls::task t5(
		orig_corrected_interp,
		orig_corrected_interp_q,
		out_orig_corrected_q
		);
}

