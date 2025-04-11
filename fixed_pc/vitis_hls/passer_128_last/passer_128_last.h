#include "hls_task.h"

#include "ap_int.h"
#include "hls_stream.h"
#include "ap_axi_sdata.h"

const int width = 128;
typedef ap_int<width> data;
typedef hls::axis<data, 0, 0, 0> data_packet;

typedef struct {
	data data;
	bool last;
} data_last;

typedef struct {
	ap_int<32> send;
} config1;

void passer_128_last(
	hls::stream<data_packet> &in_q,
	hls::stream<config1> &config_in_q,
	hls::stream<data_packet> &out_q
	);
