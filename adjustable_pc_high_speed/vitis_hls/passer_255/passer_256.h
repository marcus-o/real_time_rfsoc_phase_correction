#include "hls_task.h"

#include "ap_int.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"

const int width = 16*8*2;
typedef ap_int<width> data;

typedef struct {
	ap_int<32> send;
} config1;

void passer_256(
	hls::stream<data> &in_q,
	hls::stream<config1> &config_in_q,
	hls::stream<data> &out_q
	);
