#include "ap_int.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"
#include "ap_fixed.h"
#include "hls_math.h"

const char adc_width = 16;
typedef ap_int<adc_width> adc_data;

typedef struct {
	ap_int<adc_width> v1;
	ap_int<adc_width> v2;
} adc_data_two_val;

typedef ap_fixed<adc_width*2, adc_width> fp;
typedef ap_fixed<adc_width*3, adc_width*2> fp_long;
typedef ap_fixed<adc_width*4, adc_width*2> fp_small_long;

void saturate_stream_dr(
		hls::stream<adc_data_two_val> &out_q
		);
