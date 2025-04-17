from pynq import PL
import numpy as np
import matplotlib.pyplot as plt
from pynq import allocate
import time

from overlay.phase_correction_adjustable_overlay import phase_correction_adjustable_overlay

zero = 0x00000000
one = 0x3f800000
mone = 0xbf800000
two = 0x40000000
mtwo = 0xc0000000
three = 0x40400000
mthree = 0xc0400000
half = 0x3f000000
mhalf = 0xbf000000
third = 0x3eaaaaab
mthird = 0xbeaaaaab

class phase_correction:
    
    def __init__(
            self, offset_freq = 20e3, trig_val = 500, delay_ch_2 = 0, phase_mult_ch_2 = one, hardware_avgs=100):
        self.offset_freq = offset_freq
        self.trig_val = trig_val
        self.delay_ch_2 = delay_ch_2
        self.phase_mult_ch_2 = phase_mult_ch_2
        self.hardware_avgs = hardware_avgs

        self.sampling_rate_in = 4915.2e6 / 8
        self.sampling_rate_after_hilbert = self.sampling_rate_in/2
        self.sampling_rate_after_pc = self.sampling_rate_after_hilbert*0.95
        
        # should be a multiple of 512 (64*8) b/c 512/32/2=8
        # sample multiple (burst length * samples per clock) b/c bus_bidth / value width / values per sample)
        self.ifg_samples = int(self.sampling_rate_after_hilbert/self.offset_freq*0.95*0.95/512.)*512
        
        PL.reset()
        self.base = phase_correction_adjustable_overlay('overlay/adj.bit')
        self.base.init_rf_clks()

        self.base.ip_dict
        
        self.mems = []
        
        self.radio = self.base.radio
        self.pc_averager = self.radio.pc_averager
        self.pc_averager_2 = self.radio.pc_averager_2
        self.dma_passer = self.radio.dma_passer
        self.dma_passer_2 = self.radio.dma_passer_2
        self.writer_log = self.radio.writer_log
        self.writer_orig = self.radio.writer_orig
        self.writer_orig_corrected = self.radio.writer_orig_corrected
        self.writer_orig_2 = self.radio.writer_orig_2
        self.writer_orig_corrected_2 = self.radio.writer_orig_corrected_2
        self.trigger_worker = self.radio.trigger_worker
        self.measure_worker = self.radio.measure_worker
        self.input_passer_double = self.radio.input_passer_double
        
        self.read_avg_cnt = 0
        
    def setup_pl_ddr_buffer(
            self,
            length, data_type, target_writer,
            target_register_offset_1, target_register_offset_2,
            target_register_offset_1b=None, target_register_offset_2b=None):
        mem = allocate(shape=(length,), dtype=data_type, target=self.base.ddr.ddr4_0)
        print('buffer address, something wrong if not >= 0x1000000000: ', hex(mem.physical_address))
        mem[:] = np.zeros(length, dtype=data_type)
        mem.flush()
        target_writer.write(
            target_register_offset_1.address,
            int(mem.physical_address & 0x00000000ffffffff))
        target_writer.write(
            target_register_offset_2.address,
            int(mem.physical_address/2**32))
        if not target_register_offset_1b==None:
            target_writer.write(
                target_register_offset_1b.address,
                int(mem.physical_address & 0x00000000ffffffff))
        if not target_register_offset_2b==None:
            target_writer.write(
                target_register_offset_2b.address,
                int(mem.physical_address/2**32))
        
        self.mems.append(mem)
        return mem
    
    def disable_averager(self, averager):
        # configures averages (set demanded_avgs to 0 to avoid memory access but must set num_samples to avoid backpressure)
        averager.write(averager.register_map.num_samples.address, self.ifg_samples)
        averager.write(averager.register_map.demanded_avgs.address, 0)
        
    def setup_averager(self, averager):
        # allocates two ddr buffers (avg and final result)
        # data type is 2*int32 (real+imag)

        avg_mem = self.setup_pl_ddr_buffer(
            length=self.ifg_samples*2,
            data_type=np.int32,
            target_writer=averager,
            target_register_offset_1=averager.register_map.avg_mem_offset_1,
            target_register_offset_2=averager.register_map.avg_mem_offset_2)
        
        result_mem = self.setup_pl_ddr_buffer(
            length=self.ifg_samples*2,
            data_type=np.int32,
            target_writer=averager,
            target_register_offset_1=averager.register_map.result_mem_offset_1,
            target_register_offset_2=averager.register_map.result_mem_offset_2)

        # configures averages
        averager.write(averager.register_map.num_samples.address, self.ifg_samples)
        averager.write(averager.register_map.demanded_avgs.address, self.hardware_avgs)
        
        return result_mem

    def setup_passer(self, passer):
        # needs to be large enough for at least one detuning plus the measure worker time (3500 clocks)
        # if the measurement starts at a bad moment, the delay until the first ifg also must fit
        # therefore, 3*ifg_samples is comfortable
        self.process_buffer_length = 3*self.ifg_samples*2
        
        buffer_mem = self.setup_pl_ddr_buffer(
            length=self.process_buffer_length,
            data_type=np.int16,
            target_writer=passer,
            target_register_offset_1=passer.register_map.mem_w_offset_1,
            target_register_offset_2=passer.register_map.mem_w_offset_2,
            target_register_offset_1b=passer.register_map.mem_r_offset_1,
            target_register_offset_2b=passer.register_map.mem_r_offset_2)

        passer.write(passer.register_map.num_samples.address, self.process_buffer_length)
        
    def setup_measure_trigger_worker(self):
        # configure measure worker and averager to the correct sample length and delay and trig value
        self.measure_worker.write(
            self.measure_worker.register_map.num_samples.address,
            self.ifg_samples)
        self.measure_worker.write(
            self.measure_worker.register_map.delay_ch_2.address,
            self.delay_ch_2)
        self.measure_worker.write(
            self.measure_worker.register_map.phase_mult_ch_2.address,
            self.phase_mult_ch_2)
        self.trigger_worker.write(
            self.trigger_worker.register_map.trig_val_sq.address,
            self.trig_val**2)
        
    def start(self):
        # turns on all writer ips (and auto restart) (all must be on to avoid backpressure deadlock)
        self.pc_averager.write(self.pc_averager.register_map.CTRL.address, 0x81)
        self.writer_log.write(self.writer_log.register_map.CTRL.address, 0x81)
        self.writer_orig.write(self.writer_orig.register_map.CTRL.address, 0x81)
        self.writer_orig_corrected.write(self.writer_orig_corrected.register_map.CTRL.address, 0x81)
        self.pc_averager_2.write(self.pc_averager_2.register_map.CTRL.address, 0x81)
        self.writer_orig_2.write(self.writer_orig_2.register_map.CTRL.address, 0x81)
        self.writer_orig_corrected_2.write(self.writer_orig_corrected_2.register_map.CTRL.address, 0x81)
        self.measure_worker.write(self.measure_worker.register_map.CTRL.address, 0x81)
        
    def setup_averaging(self):

        # this needs a dbto file that cannot be applied at runtime
        # read overl/insert_dtbo.py
        # https://discuss.pynq.io/t/pynq3-0-1-allocate-ddr4-returns-buffer-outside-of-address-range/4918/7

        self.result_mem = self.setup_averager(self.base.radio.pc_averager)
        self.result_mem2 = self.setup_averager(self.base.radio.pc_averager_2)
        self.setup_passer(self.base.radio.dma_passer)
        self.setup_passer(self.base.radio.dma_passer_2)
        self.setup_measure_trigger_worker()

        self.start()

    def start_averaging(self):
        # turns on the data stream
        self.input_passer_double.write(self.input_passer_double.register_map.send.address, 1)
        self.time_1 = time.time()
        # tells averager to write the result
        self.pc_averager.write(self.pc_averager.register_map.write_in.address, int(self.read_avg_cnt+1))
        self.pc_averager_2.write(self.pc_averager_2.register_map.write_in.address, int(self.read_avg_cnt+1))
        
    def read_avg(self):
        while(self.pc_averager.read(self.pc_averager.register_map.write_out.address) < self.read_avg_cnt+1):
            pass
        while(self.pc_averager_2.read(self.pc_averager_2.register_map.write_out.address) < self.read_avg_cnt+1):
            pass
        self.read_avg_cnt = self.read_avg_cnt + 1
        
        self.time_2 = time.time()
        print(
            'meas no:', self.read_avg_cnt,
            'meas time avg:', np.round((self.time_2-self.time_1)*1e6), ' us ',
            'meas time single:', np.round((self.time_2-self.time_1)*1e6/self.hardware_avgs), ' us')
        self.time_1 = self.time_2

        self.result_mem.invalidate()
        data = np.copy(self.result_mem)
        self.result_mem2.invalidate()
        data2 = np.copy(self.result_mem2)

        self.pc_averager.write(self.pc_averager.register_map.write_in.address, int(self.read_avg_cnt+1))
        self.pc_averager_2.write(self.pc_averager_2.register_map.write_in.address, int(self.read_avg_cnt+1))
        
        data = data[::2] + 1j*data[1::2]
        data2 = data2[::2] + 1j*data2[1::2]

        return data, data2
        
    def stop(self):
        self.input_passer_double.write(self.input_passer_double.register_map.send.address, 0)

    def setup_writer(self,writer, num_write_samples):
        # allocates a ddr buffers
        # data type is 2*int16 (real+imag)
        result_mem = self.setup_pl_ddr_buffer(
            length=num_write_samples*2,
            data_type=np.int16,
            target_writer=writer,
            target_register_offset_1=writer.register_map.result_mem_offset_1,
            target_register_offset_2=writer.register_map.result_mem_offset_2) 
        
        # tells writer to write the result
        writer.write(
            writer.register_map.num_samples.address, int(num_write_samples))
        writer.write(
            writer.register_map.write_in.address, 1)

        return result_mem
        
    def setup_full_data_writing(self, num_write_samples, corrected=True):

        # should be a multiple of 1024 (64*16) b/c 512/16/2=16
        num_write_samples = int(num_write_samples/1024)*1024
        
        if corrected:
            self.result_mem = self.setup_writer(self.writer_orig_corrected, num_write_samples)
            self.result_mem2 = self.setup_writer(self.writer_orig_corrected_2, num_write_samples)
        else:
            self.result_mem = self.setup_writer(self.writer_orig, num_write_samples)
            self.result_mem2 = self.setup_writer(self.writer_orig_2, num_write_samples)
        
        self.disable_averager(self.pc_averager)
        self.disable_averager(self.pc_averager_2)
        self.setup_passer(self.dma_passer)
        self.setup_passer(self.dma_passer_2)
        self.setup_measure_trigger_worker()
        
        self.start()
        
    def read_full_data(self, corrected=True):
        # turns on the data stream
        self.input_passer_double.write(self.input_passer_double.register_map.send.address, 1)
        if corrected:
            while(self.writer_orig_corrected.read(self.writer_orig_corrected.register_map.write_out.address) < 1):
                pass
            while(self.writer_orig_corrected_2.read(self.writer_orig_corrected_2.register_map.write_out.address) < 1):
                pass
        else:
            while(self.writer_orig.read(self.writer_orig.register_map.write_out.address) < 1):
                pass
            while(self.writer_orig_2.read(self.writer_orig_2.register_map.write_out.address) < 1):
                pass
        self.input_passer_double.write(self.input_passer_double.register_map.send.address, 0)

        self.result_mem.invalidate()
        self.result_mem2.invalidate()
        
        data = np.copy(self.result_mem)
        data2 = np.copy(self.result_mem2)

        data = data[::2] + 1j*data[1::2]
        data2 = data2[::2] + 1j*data2[1::2]
        
        return data, data2
    
    def setup_log_writer(self, writer, num_write_samples):
        # allocates a ddr buffers
        # data type is 4*float32
        result_mem = self.setup_pl_ddr_buffer(
            length=num_write_samples*4,
            data_type=np.float32,
            target_writer=writer,
            target_register_offset_1=writer.register_map.result_mem_offset_1,
            target_register_offset_2=writer.register_map.result_mem_offset_2) 
        
        # tells writer_ to write the result
        writer.write(
            writer.register_map.num_samples.address, int(num_write_samples*4))
        writer.write(
            writer.register_map.write_in.address, 1)

        return result_mem
    
    def setup_log_data_writing(self, num_write_samples):

        # should be a multiple of 1024 (64*4) b/c 512/32/4=4
        num_write_samples = int(num_write_samples/256)*256

        self.result_mem = self.setup_log_writer(self.writer_log, num_write_samples)

        self.setup_passer(self.dma_passer)
        self.setup_passer(self.dma_passer_2)
        self.setup_measure_trigger_worker()
        
        self.disable_averager(self.pc_averager)
        self.disable_averager(self.pc_averager_2)
        
        self.start()
        
    def read_log_data(self):
        # turns on the data stream
        self.input_passer_double.write(self.input_passer_double.register_map.send.address, 1)
        while(self.writer_log.read(self.writer_log.register_map.write_out.address) < 1):
            pass
        self.input_passer_double.write(self.input_passer_double.register_map.send.address, 0)

        self.result_mem.invalidate()
        data = np.copy(self.result_mem)

        delta_times = data[::4]
        phases = data[1::4]
        center_freqs = data[2::4]*self.sampling_rate_after_hilbert+self.sampling_rate_after_hilbert/2
        spacers = data[3::4]
        
        return delta_times, phases, center_freqs