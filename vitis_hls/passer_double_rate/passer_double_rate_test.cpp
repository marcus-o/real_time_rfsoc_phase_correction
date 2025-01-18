#include "passer_double_rate.h"
#include "stdint.h"

int main(){

	hls::stream<adc_data> in_q;
	hls::stream<config1> config_q;
	hls::stream<adc_data> out_q;

	config1 config_temp;

	int pkg_cnt = 0;

	config_temp.send = 0;
	config_q.write(config_temp);
    for (int cnt=0; cnt<20; cnt++){
    	adc_data in = adc_data(cnt);
    	in_q.write(in);
    	passer_double_rate(in_q, out_q, config_q);
    }

	config_temp.send = 1;
	config_q.write(config_temp);
    for (int cnt=25; cnt<50; cnt++){
    	adc_data in = adc_data(cnt);
    	in_q.write(in);
    	passer_double_rate(in_q, out_q, config_q);
    }

	config_temp.send = 0;
	config_q.write(config_temp);
    for (int cnt=52; cnt<78; cnt++){
    	adc_data in = adc_data(cnt);
    	in_q.write(in);
    	passer_double_rate(in_q, out_q, config_q);
    }

    for (int cnt=0; cnt<100; cnt++){
    	adc_data out;
        if (out_q.read_nb(out)){
        	pkg_cnt++;
			printf("out_content:%d \r", out.to_int());
        }
    }
    printf("pkg_cnt:%d \r", int(pkg_cnt));
    return 0;
}
