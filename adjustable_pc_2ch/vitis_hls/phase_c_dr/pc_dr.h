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


// three is conservative but currently we have enough bram
// 2.5 is enough if measure worker only takes 0.5 times the offset period
const int wait_buf_size = 3*13824;

// size of the windows used to determine the parameter for phase correction
// in units of t_unit
// size of the first moment calculation for interferogram time center
const int size_ifg_2 = 100;
// size of the fft calculation for interferogram carrier frequency
// must be power of 2
const int size_spec_2 = 256;

// data type for the hilbert worker to shift to zero frequency
typedef ap_int<4> single_digit;

//data type for time
typedef ap_int<48> time_int;

// adc params and data types
const char adc_width = 16;
typedef ap_int<adc_width> adc_data;
typedef std::complex<adc_data> adc_data_compl;
typedef hls::axis<adc_data_compl, 0, 0, 0> adc_data_compl_packet;

// input data (2 samples per clock)
typedef struct {
	ap_int<adc_width> v1;
	ap_int<adc_width> v2;
} adc_data_two_val;

typedef ap_int<adc_width*2> adc_data_double_length;
typedef std::complex<adc_data_double_length> adc_data_double_length_compl;
typedef hls::axis<adc_data_double_length_compl, 0, 0, 0> adc_data_double_length_compl_packet;

// fft only handles one fractional bit
typedef ap_fixed<adc_width, 1> fp_short;
typedef std::complex<fp_short> fp_compl_short;

// sine and cosine only need 0-2pi input
typedef ap_fixed<adc_width+3, 3> fp_short2;

// adc width is really only 14 bit
// hilbert should not be larger than adc width, so keep length and add fractional bits
typedef ap_fixed<adc_width*2, adc_width> fp;
typedef std::complex<fp> fp_compl;
typedef ap_fixed<adc_width*3, adc_width*2> fp_long;
// datatype mainly for timing which can be large values
// 48 bit integer to allow really large times (200 hrs)
typedef ap_fixed<adc_width*4, adc_width*3> fp_time;
typedef std::complex<fp_long> fp_compl_long;
// datatype mainly for slopes which can be small
typedef ap_fixed<adc_width*3, adc_width> fp_small;
typedef std::complex<fp_small> fp_small_compl;
// datatype for calculations combining large and small values
typedef ap_fixed<adc_width*4, adc_width*2> fp_small_long;

// downsampling rate to allow for rep rate changes
const float inv95 = 1./0.95;

// structs to send data with 512 bit width (8 or 16 samples)
// stage must roll over at the last sample of vec
// -> vec length = 2^stage_bit_width
typedef ap_uint<3> adc_data_double_length_compl_vec8_stage;
typedef hls::vector<adc_data_double_length_compl, 8> adc_data_double_length_compl_vec8;
typedef ap_uint<4> adc_data_compl_vec16_stage;
typedef hls::vector<adc_data_compl, 16> adc_data_compl_vec16;

// communication data structure from measure to process worker
typedef struct {
	time_int center_time_prev = 0;
	time_int center_time_observed = 0;
	fp_time center_time_prev_exact = 0;
	fp_time center_time_observed_exact = 0;
	fp_time start_sending_time = 0;
	float phase_slope_pi = 0;
	fp sampling_time_unit = 0;
	float center_phase_prev_pi = 0;
	fp center_freq0 = 0;
	int retain_samples = 0;
	int phase_mult = 1;
} correction_data_type;

// communication data structure from primer to process worker
typedef struct {
	adc_data_compl val;
	time_int time_current;
	correction_data_type correction_data;
} process_data_type;

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
		hls::stream<adc_data_two_val> &in_sig_q,
		hls::stream<adc_data_compl> &out_sig_h_q);

void pc_dr(
		hls::stream<adc_data_compl> &proc1_buf_in_q,
		hls::stream<adc_data_compl_vec16> &out_orig_q,
		hls::stream<adc_data_compl_vec16> &out_orig_corrected_q,
		hls::stream<adc_data_double_length_compl_vec8> &avg_q,
		hls::stream<correction_data_type> &in_correction_data1_q
		);

void trigger_worker(
		hls::stream<adc_data_compl> &in_q,
		hls::stream<adc_data_compl> &proc1_buf_out_q,
		hls::stream<fp_compl> &out_meas_ifg_q,
		hls::stream<time_int> &out_meas_ifg_time_q,
		int trig_val_sq
		);

void dma_passer(
		hls::stream<adc_data_compl> &in_q,
		adc_data_compl_vec16 *mem_w,
		adc_data_compl_vec16 *mem_r,
		const int num_samples,
		hls::stream<adc_data_compl> &out_q
		);

void measure_worker(
		hls::stream<fp_compl> &in_ifg_q,
		hls::stream<time_int> &in_ifg_time_q,
		hls::stream<correction_data_type> &out_correction_data_q,
		hls::stream<correction_data_type> &out_correction_data_ch_2_q,
		hls::stream<log_data_vec4> &out_log_data_q,
		int num_samples,
		int delay_ch_2,
		int phase_mult_ch_2
		);

void pc_averager(
		hls::stream<adc_data_double_length_compl_vec8> &avg_q,
		adc_data_double_length_compl_vec8 *avg_mem,
		adc_data_double_length_compl_vec8 *result_mem,
		const int num_samples,
		const int demanded_avgs,
		const int write_in,
		int *write_out
		);

void writer(
		hls::stream<ap_int<512>> &in_q,
		ap_int<512> *result_mem,
		const int num_samples,
		const int write_in,
		int *write_out
		);

#endif
