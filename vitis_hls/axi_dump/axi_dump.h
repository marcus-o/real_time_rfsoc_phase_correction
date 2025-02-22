#include "ap_int.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"
#include "ap_fixed.h"

const char adc_width = 16;

typedef ap_fixed<adc_width*3, adc_width*2> fp_long;
typedef std::complex<fp_long> fp_compl_long;
typedef hls::axis<fp_compl_long, 0, 0, 0> fp_compl_long_data_packet;

void axi_dump(
		hls::stream<fp_compl_long_data_packet> &in_stream
	);
