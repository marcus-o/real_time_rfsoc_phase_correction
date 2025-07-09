#include "pc_dr.h"

adc_data_double_length abs_sq(adc_data_compl a){
	#pragma HLS inline
	return a.real()*a.real() + a.imag()*a.imag();
	}

void trig_worker(
		hls::stream<adc_data_compl_vec> &in_sig_h_q,
		hls::stream<adc_data_compl_vec> &out_proc_q,
		hls::stream<adc_data_compl_vec, 100> &out_ifg_q,
		hls::stream<time_int, 100> &out_ifg_time_q,
		hls::stream<adc_data_double_length, 10> &config_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

	const int buf_size1 = (size_ifg_2 + size_spec_2)/samples_per_clock;

	static hls::stream<adc_data_compl_vec, buf_size1+10> inc_buf;
	#pragma HLS reset variable=inc_buf off

	static time_int time_current = 0;
	#pragma HLS reset variable=time_current
	static int remaining_packets = 0;
	#pragma HLS reset variable=remaining_packets
	static bool prev_trig = false;
	#pragma HLS reset variable=prev_trig
	static bool first_trig = false;
	#pragma HLS reset variable=first_trig
	static int loop_cnt = 0;
	#pragma HLS reset variable=loop_cnt
	static adc_data_double_length trig_val_sq = adc_data_double_length(1000*1000);
	#pragma HLS reset variable=trig_val_sq

	// reads and writes
	// configure trigger
	adc_data_double_length trig_val_sq_in;
	if(config_q.read_nb(trig_val_sq_in))
		trig_val_sq = trig_val_sq_in;

	adc_data_compl_vec cur_write_val_vec = in_sig_h_q.read();
	inc_buf.write(cur_write_val_vec);
	hls::vector<adc_data_double_length, samples_per_clock> cur_write_val_abs_sq_vec;
	for (char cnt = 0; cnt < samples_per_clock; cnt++)
	{
		#pragma HLS UNROLL
		cur_write_val_abs_sq_vec[cnt] = abs_sq(cur_write_val_vec[cnt]);
	}
	
	// do not send if not enough information is available
	if(loop_cnt < buf_size1){
		loop_cnt++;
	} else {
		// pass on the data
		adc_data_compl_vec cur_read_val_vec = inc_buf.read();

		bool cur_trig = false;
		for (char cnt = 0; cnt < samples_per_clock; cnt++){
			#pragma HLS UNROLL
			if (cur_write_val_abs_sq_vec[cnt] > trig_val_sq){
				cur_trig = true;
				first_trig = true;
			}
		}

		if(!remaining_packets){
			if (cur_trig && (!prev_trig)){
				remaining_packets = 2*buf_size1;
			}
		}else{
			out_ifg_q.write(cur_read_val_vec);
			if(remaining_packets == 2*buf_size1){
				out_ifg_time_q.write(time_current);
			}
			remaining_packets -= 1;
		}
		// send on data after first trigger, set time zero
		if(first_trig){
			out_proc_q.write(cur_read_val_vec);
			time_current = time_current + samples_per_clock; // t_unit;
		}
		// save current value for next iteration
		prev_trig = cur_trig;
	}
}

void out_fifo_ifg1_meas(
	hls::stream<adc_data_compl_vec, 100> &in_q,
	hls::stream<adc_data_compl_vec> &out_q
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

void in_config_worker(
	int trig_val_sq,
	hls::stream<adc_data_double_length, 10> &config_q
	){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return

	static int trig_val_sq_local = 0;
	#pragma HLS reset variable=trig_val_sq_local

	if(trig_val_sq_local != trig_val_sq){
		trig_val_sq_local = trig_val_sq;
		config_q.write(adc_data_double_length(trig_val_sq));
	}
}

// main function
void trigger_worker(
		hls::stream<adc_data_compl_vec> &in_q,
		hls::stream<adc_data_compl_vec> &proc1_buf_out_q,
		hls::stream<adc_data_compl_vec> &out_meas_ifg_q,
		hls::stream<time_int> &out_meas_ifg_time_q,
		int trig_val_sq
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return

	#pragma HLS INTERFACE axis register port = in_q
	#pragma HLS INTERFACE axis register port = proc1_buf_out_q
	#pragma HLS INTERFACE axis register port = out_meas_ifg_q
	#pragma HLS INTERFACE axis register port = out_meas_ifg_time_q

	#pragma HLS INTERFACE s_axilite register port=trig_val_sq bundle=a

	// launch all parallel tasks and their communication

	hls_thread_local hls::stream<adc_data_double_length, 10> config_q;
	hls_thread_local hls::task t1(in_config_worker, trig_val_sq, config_q);

	hls_thread_local hls::stream<adc_data_compl_vec, 100> ifg1_meas_fifo_q;
	hls_thread_local hls::stream<time_int, 100> ifg1_time_meas_fifo_q;
	hls_thread_local hls::task t2(trig_worker, in_q, proc1_buf_out_q, ifg1_meas_fifo_q, ifg1_time_meas_fifo_q, config_q);

	hls_thread_local hls::task t5d(out_fifo_ifg1_meas, ifg1_meas_fifo_q, out_meas_ifg_q);
	hls_thread_local hls::task t5e(out_fifo_ifg1_time_meas, ifg1_time_meas_fifo_q, out_meas_ifg_time_q);
}

