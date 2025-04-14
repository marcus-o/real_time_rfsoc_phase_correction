#include "pc_dr.h"

int abs_sq(adc_data_compl a){
	#pragma HLS inline
	return int(a.real())*int(a.real()) + int(a.imag())*int(a.imag());
	}

void trig_worker(
		hls::stream<adc_data_compl> &in_sig_h_q,
		hls::stream<adc_data_compl> &out_proc_q,
		hls::stream<fp_compl, 100> &out_ifg_q,
		hls::stream<time_int, 100> &out_ifg_time_q,
		hls::stream<int, 10> &config_q
		){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return
	#pragma HLS PIPELINE II=1

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
	static int trig_val_sq = 1000*1000;
	#pragma HLS reset variable=trig_val_sq

	// reads and writes
	// configure trigger
	int trig_val_sq_in;
	if(config_q.read_nb(trig_val_sq_in))
		trig_val_sq = trig_val_sq_in;

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

void in_config_worker(
	int trig_val_sq,
	hls::stream<int, 10> &config_q
	){
	#pragma HLS INTERFACE mode=ap_ctrl_none port=return

	static int trig_val_sq_local = 0;
	#pragma HLS reset variable=trig_val_sq_local

	if(trig_val_sq_local != trig_val_sq){
		trig_val_sq_local = trig_val_sq;
		config_q.write(trig_val_sq);
	}
}

// main function
void trigger_worker(
		hls::stream<adc_data_compl> &in_q,
		hls::stream<adc_data_compl> &proc1_buf_out_q,
		hls::stream<fp_compl> &out_meas_ifg_q,
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

	hls_thread_local hls::stream<int, 10> config_q;
	hls_thread_local hls::task t1(in_config_worker, trig_val_sq, config_q);

	hls_thread_local hls::stream<fp_compl, 100> ifg1_meas_fifo_q;
	hls_thread_local hls::stream<time_int, 100> ifg1_time_meas_fifo_q;
	hls_thread_local hls::task t2(trig_worker, in_q, proc1_buf_out_q, ifg1_meas_fifo_q, ifg1_time_meas_fifo_q, config_q);

	hls_thread_local hls::task t5d(out_fifo_ifg1_meas, ifg1_meas_fifo_q, out_meas_ifg_q);
	hls_thread_local hls::task t5e(out_fifo_ifg1_time_meas, ifg1_time_meas_fifo_q, out_meas_ifg_time_q);
}

