#include "ap_int.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"

const char adc_width = 16*2;

typedef ap_int<adc_width> adc_data;

typedef struct {
	ap_int<32> send;
} config1;

void passer_double_rate(
		hls::stream<adc_data> &in_q,
		hls::stream<adc_data> &out_q,
		hls::stream<config1> &config_in_q
		);
