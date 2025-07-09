`timescale 1ns / 1ps

import axi_vip_pkg::*;
import test_bd_axi_vip_0_0_pkg::*;

xil_axi_resp_t 	resp;
bit[31:0]  addr, data, base_addr = 0;

module test();
  
    // instantiate bd
    test_bd_wrapper test_bd_i();

    test_bd_axi_vip_0_0_mst_t master_agent;
    
    initial begin
        master_agent = new("master vip agent", test_bd_i.test_bd_i.axi_vip_0.inst.IF);
        master_agent.start_master();
        #500ns

        // trigger
        base_addr = 2*65536;
        // trigger height
        addr = 16;
        data = 1000;
        master_agent.AXI4LITE_WRITE_BURST(base_addr + addr,0,data,resp);

        // measurer
        base_addr = 65536;
        // sample length 13824
        addr = 16;
        data = 13824*8;
        master_agent.AXI4LITE_WRITE_BURST(base_addr + addr,0,data,resp);
        // turn on
        addr = 0;
        data = 129;
        master_agent.AXI4LITE_WRITE_BURST(base_addr + addr,0,data,resp);

        // averager
        base_addr = 0;
        // avg num samples
        addr = 16;
        data = 13824*8;
        master_agent.AXI4LITE_WRITE_BURST(base_addr + addr,0,data,resp);
        // number of avgs
        addr = 24;
        data = 5;
        master_agent.AXI4LITE_WRITE_BURST(base_addr + addr,0,data,resp);  
        // turn on
        addr = 0;
        data = 129;
        master_agent.AXI4LITE_WRITE_BURST(base_addr + addr,0,data,resp);
     
        //#300us
        //addr = 56;
        //data = 0;
        //master_agent.AXI4LITE_READ_BURST(base_addr + addr,0,data,resp);
        //$display(resp);   
        //#100us
        //master_agent.AXI4LITE_READ_BURST(base_addr + addr,0,data,resp);
        //$display(resp);
            

    end

endmodule