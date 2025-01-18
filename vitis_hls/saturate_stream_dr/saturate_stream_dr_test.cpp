#include "saturate_stream.h"

int main(){

	hls::stream<adc_data> out_q;

    for (int cnt=0; cnt<300000; cnt++)
    {
    	saturate_stream(out_q);
    	adc_data out = out_q.read();
    	//if(cnt > 155800)
    	//	printf(" out: %d \r", out.to_int());
    }
    return 0;
}
