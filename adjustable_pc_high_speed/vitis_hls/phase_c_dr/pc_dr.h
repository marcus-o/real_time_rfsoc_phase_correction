#ifndef pc_dr_H
#define pc_dr_H

#include "hls_task.h"

#include "ap_int.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"
#include "ap_fixed.h"
#include "hls_math.h"
#include "hls_vector.h"
#include "hls_fft.h"

// adc params
const char adc_width = 16;
const char samples_per_clock = 8;

typedef unsigned int uint;

// only has to save 1 interferogram plus the time the measure worker takes, so 
// 1.5 times the interferogram length at 20 kHz at no decimation should be enough
// code will support lower detunings at higher decimation
const int max_ifg_length = 13824;
const int wait_buf_size = 1.5*max_ifg_length;

// size of the windows used to determine the parameter for phase correction
// in units of t_unit
// size of the first moment calculation for interferogram time center
const int size_ifg_2 = int(256/samples_per_clock)*samples_per_clock;
// size of the fft calculation for interferogram carrier frequency
// must be power of 2
const int size_spec_2 = int(256/samples_per_clock)*samples_per_clock;

// data type for the hilbert worker to shift to zero frequency
typedef ap_int<4> single_digit;

//data type for time
typedef ap_int<46> time_int;
typedef hls::vector<time_int, samples_per_clock> time_int_vec;

// adc data types
typedef ap_int<adc_width> adc_data;
typedef hls::vector<adc_data, samples_per_clock> adc_data_vec;
typedef std::complex<adc_data> adc_data_compl;
typedef hls::vector<std::complex<adc_data>, samples_per_clock> adc_data_compl_vec;

// input data (2 samples per clock)
typedef struct {
	ap_int<adc_width> v1;
	ap_int<adc_width> v2;
} adc_data_two_val;
typedef hls::vector<adc_data_two_val, samples_per_clock> adc_data_two_val_vec;
typedef hls::vector<adc_data, 12> adc_data_vec12;
typedef hls::vector<adc_data, 16> adc_data_vec16;

typedef ap_int<adc_width*2> adc_data_double_length;
typedef std::complex<adc_data_double_length> adc_data_double_length_compl;

// fft only handles one fractional bit
typedef ap_fixed<adc_width, 1> fp_short;
typedef std::complex<fp_short> fp_compl_short;

// adc width is really only 14 bit
// hilbert should not be larger than adc width, so keep length and add fractional bits
typedef ap_fixed<adc_width*2, adc_width> fp;
typedef hls::vector<fp, samples_per_clock> fp_vec;
typedef std::complex<fp> fp_compl;
typedef hls::vector<fp_compl, samples_per_clock> fp_compl_vec;
typedef ap_fixed<adc_width*3, adc_width*2> fp_long;
typedef std::complex<fp_long> fp_compl_long;
typedef hls::vector<fp_compl_long, samples_per_clock> fp_compl_long_vec;

// datatype mainly for timing which can be large values
// 48 bit integer to allow really large times (200 hrs)
typedef ap_fixed<adc_width*4, adc_width*3> fp_time;
typedef hls::vector<fp_time, samples_per_clock> fp_time_vec;

// downsampling rate to allow for rep rate changes
const float inv95 = 1./0.95;

// structs to send data with 512 bit width (8 or 16 samples)
// stage must roll over at the last sample of vec
// -> vec length = 2^stage_bit_width
typedef hls::vector<adc_data_double_length_compl, 8> adc_data_double_length_compl_vec8;
typedef hls::vector<adc_data_compl, 16> adc_data_compl_vec16;

typedef struct {
	adc_data_compl_vec data;
	char no_samples;
} adc_data_compl_vec_info;

// communication data structure from measure to process worker primer
typedef struct {
	time_int center_time_prev = 0; //
	time_int center_time_observed = 0; //
	fp_time start_sending_time = 0;
	float phase_slope_pi = 0;
	fp sampling_time_unit = 0;
	float center_phase_prev_pi = 0;
	int retain_samples = 0;
	int phase_mult = 1;
} correction_data_type;

// communication data structure from primer to process worker
typedef struct {
	adc_data_compl_vec signal_vec;
	correction_data_type correction_data;
	correction_data_type correction_data_next;
	hls::vector<bool, samples_per_clock> next_correction;
	hls::vector<fp_time, samples_per_clock+1> cnt_x_times;
} process_data_type;

// communication data structure from process worker to interpolator
typedef struct {
	fp_compl_vec signal_vec;
	hls::vector<fp_time, samples_per_clock+1> cnt_x_times;
	fp_compl prev_inc_corr;
	fp_time start_sending_time;
	int retain_samples;
} process_data_type_reduced;

// communication data structure for logging
typedef struct {
	float delta_time_exact = 0;
	float phase = 0;
	float center_freq = 0;
	float phase_change_pi = 0;
} log_data_type;
typedef ap_uint<2> log_data_stage;
// 512 bit width for fast dma
typedef hls::vector<log_data_type, 4> log_data_vec4;

// fft parameters and config data types
struct param1: hls::ip_fft::params_t {
	static const unsigned ordering_opt = hls::ip_fft::natural_order;
	static const bool has_nfft = false;
	static const unsigned max_nfft = 9;
	static const unsigned input_width = adc_width;
	static const unsigned output_width = adc_width;
	// static const unsigned scaling_opt = hls::ip_fft::scaled;
	// static const unsigned config_width = 16;
	static const unsigned scaling_opt = hls::ip_fft::block_floating_point;
	static const unsigned config_width = 8;
	static const unsigned rounding_opt = hls::ip_fft::truncation;
};
typedef hls::ip_fft::config_t<param1> config_t;
typedef hls::ip_fft::status_t<param1> status_t;

//main function definition
void hilbert_transform(
		hls::stream<adc_data_two_val_vec> &in_sig_q,
		hls::stream<adc_data_compl_vec> &out_sig_h_q,
		hls::stream<adc_data_compl_vec> &out_orig_q
	);

void trigger_worker(
		hls::stream<adc_data_compl_vec> &in_q,
		hls::stream<adc_data_compl_vec> &proc1_buf_out_q,
		hls::stream<adc_data_compl_vec> &out_meas_ifg_q,
		hls::stream<time_int> &out_meas_ifg_time_q,
		int trig_val_sq
		);

void measure_worker(
		hls::stream<adc_data_compl_vec> &in_ifg_q,
		hls::stream<time_int> &in_ifg_time_q,
		hls::stream<correction_data_type> &out_correction_data_q,
		hls::stream<correction_data_type> &out_correction_data_ch_2_q,
		hls::stream<log_data_vec4> &out_log_data_q,
		int num_samples,
		int delay_ch_2,
		int phase_mult_ch_2
		);

void pc_dr(
		hls::stream<adc_data_compl_vec> &proc1_buf_in_q,
		hls::stream<adc_data_compl_vec_info> &out_orig_corrected_q,
		hls::stream<adc_data_compl_vec_info> &avg_q,
		hls::stream<correction_data_type> &in_correction_data1_q
		);

void process_worker_primer(
		hls::stream<adc_data_compl_vec> &in_q,
		hls::stream<correction_data_type, 5> &in_correction_data_q,
		hls::stream<process_data_type, 2> &out_q
		);

void process_worker(
		hls::stream<process_data_type, 2> &in_q,
		hls::stream<process_data_type_reduced, 2> &avg_interp_q,
		hls::stream<process_data_type_reduced, 2> &orig_corrected_interp_q
		);

void avg_interp(
		hls::stream<process_data_type_reduced, 2> &in_q,
		hls::stream<adc_data_compl_vec_info> &avg_q
		);

void orig_corrected_interp(
		hls::stream<process_data_type_reduced, 2> &in_q,
		hls::stream<adc_data_compl_vec_info> &out_orig_corrected_q
		);

void dropperandfifo(
		hls::stream<adc_data_compl_vec_info> &in_q,
		hls::stream<adc_data_compl_vec16> &out_q
		);

void out_dropper(
		hls::stream<adc_data_compl_vec_info> &in_q,
		hls::stream<adc_data_compl_vec16> &out_q
		);

void pc_averager(
		hls::stream<adc_data_compl_vec16> &in_q,
		int num_samples,
		int demanded_avgs,
		hls::stream<adc_data_double_length_compl_vec8> &out_q
		);

void c_complex8_to_16(
		hls::stream<adc_data_compl_vec> &in_sig_q,
		hls::stream<adc_data_compl_vec16> &out_sig_h_q);

void writer(
		hls::stream<ap_int<512>> &in_q,
		ap_int<512> *result_mem,
		const int num_samples,
		const int write_in,
		int *write_out
		);

#endif
