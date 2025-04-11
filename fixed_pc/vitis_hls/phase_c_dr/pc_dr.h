#include "hls_task.h"

#include "ap_int.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"
#include "ap_fixed.h"
#include "hls_math.h"

#include "hls_fft.h"

// use case description
const int sampling_rate_in = 614e6;;
const int sampling_rate_after_hilbert = sampling_rate_in/2;
const int offset_freq = 20e3;;

// three is conservative but currently we have enough bram
// 2.5 is enough if measure worker only takes 0.5 times the offset period
const int wait_buf_size = int(2.5*(sampling_rate_after_hilbert/offset_freq)/2)*2;

// size of the windows used to determine the parameter for phase correction
// in units of t_unit
// size of the first moment calculation for interferogram time center
const int size_ifg_2 = 100;
// size of the fft calculation for interferogram carrier frequency
// must be power of 2
const int size_spec_2 = 256;

//trigger value (max is 8192)
const int trig_val = 2000;

// sampling rate
// i believe this can stay one when you are aware of it in post
// const double t_unit_in = 1.;
// const int t_unit = 1;
// const int t_unit_inv = 1;

// length of samples to average, scale by the downsampling 0.95 and 0.95 to cut front and back
const int retain_samples = int(int(sampling_rate_after_hilbert/offset_freq*0.95*0.95/4.)*4);
// length when outputting unaveraged data
const int orig_retain_samples = int(int(15350000./4.)*4);

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

// struct to send unaveraged data with 128 bit width (4 samples)
typedef struct {
	adc_data_compl v1;
	adc_data_compl v2;
	adc_data_compl v3;
	adc_data_compl v4;
} adc_data_compl_4sampl;
typedef hls::axis<adc_data_compl_4sampl, 0, 0, 0> adc_data_compl_4sampl_packet;

// struct to send averaged data with 128 bit width (2 samples)
typedef struct {
	adc_data_double_length_compl v1;
	adc_data_double_length_compl v2;
}adc_data_double_length_compl_2sampl;
typedef hls::axis<adc_data_double_length_compl_2sampl, 0, 0, 0> adc_data_double_length_compl_2sampl_packet;

// struct to send averaged data with 256 bit width (4 samples)
typedef struct {
	adc_data_double_length_compl v1;
	adc_data_double_length_compl v2;
	adc_data_double_length_compl v3;
	adc_data_double_length_compl v4;
}adc_data_double_length_compl_4sampl;
typedef hls::axis<adc_data_double_length_compl_4sampl, 0, 0, 0> adc_data_double_length_compl_4sampl_packet;

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
} correction_data_type;

// communication data structure from measure to process worker
typedef struct {
	adc_data_compl val;
	time_int time_current;
	correction_data_type correction_data;
} process_data_type;

// communication data structure for logging to dma
typedef struct {
	float delta_time = 0;
	float phase = 0;
	float center_freq = 0;
	float spacer = 0;
} log_data_type;
typedef hls::axis<log_data_type, 0, 0, 0> log_data_packet;
const int ld_packets = 20000;


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
void pc_dr(
	hls::stream<adc_data_two_val> &in_q,
	hls::stream<adc_data_double_length_compl_2sampl_packet> &out_q,
	hls::stream<ap_int<32>> &num_avgs_q,
	hls::stream<log_data_packet> &out_log_data_q,
	hls::stream<adc_data_compl_4sampl_packet> &out_orig_q,
	hls::stream<adc_data_compl_4sampl_packet> &out_orig_corrected_q
	);
