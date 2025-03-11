#include "axi_dump.h"

void axi_dump(
		hls::stream<fp_compl_long_data_packet> &in_stream
		){
	#pragma HLS INTERFACE axis port=in_stream
	#pragma HLS interface mode=ap_ctrl_none port=return
	#pragma HLS pipeline II=1

	fp_compl_long_data_packet tmp = in_stream.read();
}
