#include "ap_int.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"

typedef struct {
	ap_int<32> send;
} config1;

void const_config_writer(
		hls::stream<config1> &config_out_q
	);
