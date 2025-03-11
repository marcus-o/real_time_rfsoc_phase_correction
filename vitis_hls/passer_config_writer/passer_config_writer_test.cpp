#include "stdint.h"
#include <ap_axi_sdata.h>
#include "ap_int.h"

#include "passer_config_writer.h"

int main(){
	hls::stream<config1> config_out;

	config1 config_temp;

	ap_int<32> send = 0;
	for(int cnt=0; cnt<20; cnt++){
		passer_config_writer(send, config_out);
		if(config_out.read_nb(config_temp))
			printf("config send:%d \r", int(config_temp.send));
	}

	send = 1;
	for(int cnt=0; cnt<20; cnt++){
		passer_config_writer(send, config_out);
		if(config_out.read_nb(config_temp))
			printf("config send:%d \r", int(config_temp.send));
	}

	send = 0;
	for(int cnt=0; cnt<20; cnt++){
		passer_config_writer(send, config_out);
		if(config_out.read_nb(config_temp))
			printf("config send:%d \r", int(config_temp.send));
	}
	return 0;
}
