#include "pc_dr.h"

int abs_sq(adc_data_compl a){
	#pragma HLS inline
	return int(a.real())*int(a.real()) + int(a.imag())*int(a.imag());
	}

void trig_worker(
		hls::stream<adc_data_compl> &in_sig_h_q,
		hls::stream<adc_data_compl> &out_proc_q,
		hls::stream<fp_compl, 100> &out_ifg_q,
		hls::stream<time_int, 100> &out_ifg_time_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
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

void process_worker_primer(
		hls::stream<adc_data_compl> &in_q,
		hls::stream<correction_data_type, 5> &in_correction_data_q,
		hls::stream<process_data_type, 10> &out_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
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
		hls::stream<adc_data_double_length_compl_vec8, 256> &avg_q,
		hls::stream<adc_data_compl_vec16, 256> &out_orig_q,
		hls::stream<adc_data_compl_vec16, 256> &out_orig_corrected_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	static fp_time sampling_time_current = 0;
	#pragma HLS reset variable=sampling_time_current
	static fp_time sampling_time_current2 = 0;
	#pragma HLS reset variable=sampling_time_current2

	static fp_compl prev_inc_corr = fp_compl(0, 0);
	#pragma HLS reset variable=prev_inc_corr

	static int send_packets = 0;
	#pragma HLS reset variable = send_packets

	static adc_data_double_length_compl_vec8 out;
	#pragma HLS reset variable=out off
	static adc_data_compl_vec16 out_orig;
	#pragma HLS reset variable=out_orig off
	static adc_data_compl_vec16 out_orig_corrected;
	#pragma HLS reset variable=out_orig_corrected off

	static adc_data_double_length_compl_vec8_stage stage = 0;
	#pragma HLS reset variable=stage
	static adc_data_compl_vec16_stage orig_stage = 0;
	#pragma HLS reset variable=orig_stage
	static adc_data_compl_vec16_stage orig_corrected_stage = 0;
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
		send_packets = cd.retain_samples;
		first = false;
	}

	//correct and send data
	float phase1_pi = cd.center_phase_prev_pi + cd.phase_slope_pi * (time_current - cd.center_time_prev).to_float(); //* t_unit
	fp phase_mod_pi = fp(phase1_pi - hls::floor(phase1_pi/2)*2);
	fp_compl correction = fp_compl(hls::cospi(-phase_mod_pi), hls::sinpi(-phase_mod_pi));
	fp_compl inc_corr = fp_compl(fp(in_val.real()), fp(in_val.imag())) * correction;

	// check if the down sampled signal has a data point between the last two samples
	averaged_corrected: if(fp_time(time_current) >= sampling_time_current)
	{
		// if yes, interpolate
		fp time_left = fp(sampling_time_current - fp_time(time_current - 1)); //-t_unit);
		fp_compl interp = prev_inc_corr + (inc_corr - prev_inc_corr) * fp_compl(time_left, 0.); // * t_unit_inv;

		// if the last packet of the current interferogram was sent, load the starting point of the next interferogram
		// otherwise, use the current sampling time unit to calculate the next sampling time point.
		// this will change at each interferogram peak to the new value
		sampling_time_current = (send_packets==1) ? cd.start_sending_time : fp_time(sampling_time_current + fp_time(cd.sampling_time_unit));
		send_packets = (send_packets==1) ? cd.retain_samples : send_packets - 1;

		// pack 8 values
		adc_data_double_length_compl val = adc_data_double_length_compl(adc_data_double_length(interp.real()), adc_data_double_length(interp.imag()));
		out[stage] = val;
		if (stage==7)
			avg_q.write(out);
		stage++;
	}

	// used to send unaveraged corrected data (full data stream), same logic as above
	unaveraged_corrected: if(time_current >= sampling_time_current2)
	{
		fp time_left = sampling_time_current2 - (time_current - 1); //-t_unit);
		fp_compl interp = prev_inc_corr + (inc_corr - prev_inc_corr) * time_left; // * t_unit_inv;
		sampling_time_current2 = sampling_time_current2 + cd.sampling_time_unit;

		// pack 16 values
		adc_data_compl val = adc_data_compl(adc_data(interp.real()), adc_data(interp.imag()));
		out_orig_corrected[orig_corrected_stage] = val;
		if (orig_corrected_stage==15){
			out_orig_corrected_q.write(out_orig_corrected);
		}
		orig_corrected_stage++;
	}

	// used to send uncorrected unaveraged data (full data stream)
	unaveraged_uncorrected: {
		// pack 16 values
		adc_data_compl val = in_val;
		out_orig[orig_stage] = val;
		if(orig_stage==15){
			out_orig_q.write(out_orig);
		}
		orig_stage++;
	}
	// data for the next iteration
	prev_inc_corr = inc_corr;
}

void out_fifo_avg(
		hls::stream<adc_data_double_length_compl_vec8, 256> &in_q,
		hls::stream<adc_data_double_length_compl_vec8> &out_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	out_q.write(in_q.read());
}

void out_fifo_orig(
		hls::stream<adc_data_compl_vec16, 256> &in_q,
		hls::stream<adc_data_compl_vec16> &out_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	out_q.write(in_q.read());
}

void out_fifo_orig_corrected(
		hls::stream<adc_data_compl_vec16, 256> &in_q,
		hls::stream<adc_data_compl_vec16> &out_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	out_q.write(in_q.read());
}

void out_fifo_log(
		hls::stream<log_data_vec4, 256> &in_q,
		hls::stream<log_data_vec4> &out_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	out_q.write(in_q.read());
}

void in_fifo_correction(
	hls::stream<correction_data_type> &in_q,
	hls::stream<correction_data_type, 5> &out_q
	){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	out_q.write(in_q.read());
}

void out_fifo_ifg1_meas(
	hls::stream<fp_compl, 100> &in_q,
	hls::stream<fp_compl> &out_q
	){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	out_q.write(in_q.read());
}

void out_fifo_ifg1_time_meas(
	hls::stream<time_int, 100> &in_q,
	hls::stream<time_int> &out_q
	){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	out_q.write(in_q.read());
}

// main function
void pc_dr(
		hls::stream<adc_data_compl> &in_q,
		hls::stream<adc_data_compl> &proc1_buf_out_q,
		hls::stream<adc_data_compl> &proc1_buf_in_q,
		hls::stream<adc_data_compl_vec16> &out_orig_q,
		hls::stream<adc_data_compl_vec16> &out_orig_corrected_q,
		hls::stream<adc_data_double_length_compl_vec8> &avg_q,
		hls::stream<correction_data_type> &in_correction_data1_q,
		hls::stream<fp_compl> &out_meas_ifg_q,
		hls::stream<time_int> &out_meas_ifg_time_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS INTERFACE axis register port = in_q
	#pragma HLS INTERFACE axis register port = proc1_buf_out_q
	#pragma HLS INTERFACE axis register port = proc1_buf_in_q
	#pragma HLS INTERFACE axis register port = out_orig_q
	#pragma HLS INTERFACE axis register port = out_orig_corrected_q
	#pragma HLS INTERFACE axis register port = avg_q
	#pragma HLS INTERFACE axis register port = in_correction_data1_q
	#pragma HLS INTERFACE axis register port = out_meas_ifg_q
	#pragma HLS INTERFACE axis register port = out_meas_ifg_time_q

	// launch all parallel tasks and their communication

    //hls_thread_local hls::stream<adc_data_compl, wait_buf_size> proc1_buf_out_q;
	hls_thread_local hls::stream<fp_compl, 100> ifg1_meas_fifo_q;
	hls_thread_local hls::stream<time_int, 100> ifg1_time_meas_fifo_q;
	hls_thread_local hls::task t2(trig_worker, in_q, proc1_buf_out_q, ifg1_meas_fifo_q, ifg1_time_meas_fifo_q);
	hls_thread_local hls::task t5d(out_fifo_ifg1_meas, ifg1_meas_fifo_q, out_meas_ifg_q);
	hls_thread_local hls::task t5e(out_fifo_ifg1_time_meas, ifg1_time_meas_fifo_q, out_meas_ifg_time_q);

	hls_thread_local hls::stream<correction_data_type, 5> correction_data_fifo_q;
	hls_thread_local hls::task t5f(in_fifo_correction, in_correction_data1_q, correction_data_fifo_q);
	hls_thread_local hls::stream<process_data_type, 10> proc1_out_q;
	hls_thread_local hls::task t4(process_worker_primer, proc1_buf_in_q, correction_data_fifo_q, proc1_out_q);

	hls_thread_local hls::stream<adc_data_double_length_compl_vec8, 256> avg_fifo_q;
	hls_thread_local hls::stream<adc_data_compl_vec16, 256> orig_fifo_q;
	hls_thread_local hls::stream<adc_data_compl_vec16, 256> orig_corrected_fifo_q;
	hls_thread_local hls::task t4b(process_worker, proc1_out_q, avg_fifo_q, orig_fifo_q, orig_corrected_fifo_q);

	// create fifos to allow for read/write latency on the axi-ddr bus
	hls_thread_local hls::task t5a(out_fifo_orig, orig_fifo_q, out_orig_q);
	hls_thread_local hls::task t5b(out_fifo_orig_corrected, orig_corrected_fifo_q, out_orig_corrected_q);
	hls_thread_local hls::task t5c(out_fifo_avg, avg_fifo_q, avg_q);


}

