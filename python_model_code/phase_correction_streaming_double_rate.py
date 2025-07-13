# %% 2024 Marcus Ossiander TU Graz
# model phase correction code taking 
# into account the data flow in the fpga
# you need test data (load in lines 19-26) to run the code 

import numpy as np
import matplotlib.pyplot as plt
from tqdm import tqdm

import time
import copy

from queue import Queue
import threading
from functools import partial

# %% load data
t_unit_real = 1e-9
t_unit = 1
E_measured = np.loadtxt('C:/FPGA/real_time_rfsoc_phase_correction/test_data/in_1ch_acetylene_short.txt')
E_measured = E_measured/5811*0.06
t_whole = np.arange(E_measured.size) * t_unit_real

plt.plot(E_measured)
plt.show()
# %%
sampling_rate = 1e9
offset_freq = 20e3

size_ifg_2 = 50
# must be power of 2
size_spec_2 = 2**8

# prepare hilbert fir filter
# this would usually be compiled in
# as it only depends on min freq and num taps
# num taps must be odd
hilbert_numtaps = 101
mid = (hilbert_numtaps - 1) // 2  # Half of the number of taps
fir_coeffs_hilbert = np.zeros(hilbert_numtaps)
for n in range(hilbert_numtaps):
    if n == mid:
        fir_coeffs_hilbert[n] = 0
    elif (n - mid) % 2 == 1:
        fir_coeffs_hilbert[n] = 2 / (np.pi * (n - mid))


# workers start here
def sig_in_worker(sig_q, n):
    for e in E_measured[0:52450*(n+3)]:
        sig_q.put(e)


def hilbert_worker(in_sig_q, out_sig_h_q):
    buf_size0 = hilbert_numtaps
    inc_buf = np.zeros(buf_size0, dtype=np.complex128)
    cnt = 0
    while True:
        inc_buf = np.roll(inc_buf, -1)
        inc_buf[-1] = in_sig_q.get()
        in_sig_q.task_done()
        temp = (
            inc_buf[(hilbert_numtaps - 1) // 2]
            - 1j*np.sum(inc_buf * fir_coeffs_hilbert))/2
        if cnt == 0:
            out_sig_h_q.put(temp*2)
        elif cnt == 2:
            out_sig_h_q.put(-temp*2)
        elif cnt == 3:
            cnt = -1
        cnt = cnt + 1


def trig_worker(in_sig_h_q, out_proc_q, out_ifg_q, out_ifg_time_q):
    trig_val = 0.02

    t_current = 0
    buf_size1 = size_ifg_2 + size_spec_2
    inc_buf = np.zeros(buf_size1, dtype=np.complex128)
    remaining_packets = 0
    prev_write_val = 0
    write_idx = 0
    read_idx = 1

    while True:
        # incoming data and trigger detection
        e = in_sig_h_q.get()
        inc_buf[write_idx] = e
        in_sig_h_q.task_done()

        # pass on the data and time vector
        out_proc_q.put(inc_buf[read_idx])

        if (not remaining_packets):
            if (
                 (t_current > buf_size1*t_unit)
                 and (np.abs(e) > trig_val)
                 and (np.abs(prev_write_val) < trig_val)):
                remaining_packets = 2*buf_size1
        else:
            out_ifg_q.put(inc_buf[read_idx])
            out_ifg_time_q.put(t_current)
            remaining_packets -= 1

        if write_idx == buf_size1 - 1:
            write_idx = 0
        else:
            write_idx += 1

        if read_idx == buf_size1 - 1:
            read_idx = 0
        else:
            read_idx += 1

        prev_write_val = e
        t_current = t_current + t_unit


class correction_data_type:
    def __init__(self):
        self.center_time_prev = 0.
        self.center_time_observed = 0.
        self.phase_slope = 0.
        self.sampling_time_unit = 0.
        self.center_phase_prev = 0.
        self.center_freq = 0.


def measure_worker(in_ifg_q, in_ifg_time_q, out_correction_data_q):

    cd = correction_data_type()

    center_time_prev = 0
    center_phase_prev = 0

    cnt_proc_loops = 0

    buf_size2 = 2*(size_ifg_2 + size_spec_2)
    ifg_in1 = np.zeros(buf_size2, dtype=np.complex128)
    t_in1 = np.zeros(buf_size2, dtype=np.double)

    while True:
        #######################
        # first processing step
        for idx in range(buf_size2):
            ifg_in1[idx] = in_ifg_q.get()
            t_in1[idx] = in_ifg_time_q.get()
            in_ifg_q.task_done()
            in_ifg_time_q.task_done()

        proc1_sum1 = 0
        proc1_sum2 = 0
        for idx in range(size_spec_2, size_spec_2 + 2*size_ifg_2):
            proc1_sum1 += np.abs(ifg_in1[idx])**2 * (idx-size_spec_2)
            proc1_sum2 += np.abs(ifg_in1[idx])**2
        # time correction measure
        # unaligned times
        # center_time_observed = proc1_sum1 / proc1_sum2 + t_in1[size_spec_2]
        shift = int(proc1_sum1 // proc1_sum2)
        # aligned times
        center_time_observed = proc1_sum1 // proc1_sum2 + t_in1[size_spec_2]

        center_phase_observed = np.angle(ifg_in1[size_spec_2 + shift])

        fft_in = np.zeros(2*size_spec_2, dtype=np.complex128)
        for idx in range(0, 2*size_spec_2):
            fft_in[idx] = ifg_in1[idx + shift]

        # f_ifg = np.fft.fftfreq(size_spec_2*2, d=t_unit)
        spec_ifg = np.fft.fftshift(np.fft.fft(fft_in))

        proc1_sum1 = 0
        proc1_sum2 = 0
        for idx in range(spec_ifg.size):
            proc1_sum1 += (
                np.abs(spec_ifg[idx])**2 * (idx-size_spec_2)/(size_spec_2*2))
            proc1_sum2 += np.abs(spec_ifg[idx])**2
        center_freq_observed = proc1_sum1 / proc1_sum2

        if cnt_proc_loops == 0:
            center_freq0 = center_freq_observed

        delta_time = (center_time_observed - center_time_prev)
        if cnt_proc_loops == 1:
            offset_time0 = delta_time

        phase_slope = center_freq0 * 2*np.pi
        phase_applied_ps = phase_slope * delta_time

        add_phase = (
            - phase_applied_ps
            + center_phase_observed - center_phase_prev)

        add_phase = add_phase % (2*np.pi)
        if add_phase > np.pi:
            add_phase = add_phase - 2*np.pi
        # if add_phase < -np.pi:
        #    add_phase = add_phase + 2*np.pi

        add_phase_slope = add_phase / delta_time

        if cnt_proc_loops >= 1:
            sampling_time_unit = t_unit * delta_time / offset_time0 / 0.95

            cd.center_phase_prev = center_phase_prev
            cd.center_freq = center_freq_observed
            cd.phase_slope = phase_slope + add_phase_slope
            cd.center_time_prev = center_time_prev
            cd.center_time_observed = center_time_observed
            cd.sampling_time_unit = sampling_time_unit

            out_correction_data_q.put(copy.deepcopy(cd))

        center_time_prev = center_time_observed
        center_phase_prev = center_phase_observed

        cnt_proc_loops += 1
        print(cnt_proc_loops)


def process_worker(in_q, out_q, in_correction_data_q):

    buf_size3 = int(5*sampling_rate/offset_freq)
    inc_buf = np.zeros(buf_size3, dtype=np.complex128)

    time_current = 0
    sampling_time_current = 0
    prev_inc_corr = 0
    ready1 = False
    ready2 = False

    write_idx = 0
    read_idx = 1

    phase1 = 0
    phase_slope = 0
    sampling_time_unit = 0
    center_time_prev = 0
    cd = correction_data_type()

    while True:

        inc_buf[write_idx] = in_q.get()
        in_q.task_done()

        if write_idx == buf_size3 - 1:
            write_idx = 0
            print('proc 1 worker write_idx reset')
        else:
            write_idx += 1

        if not ready1:
            if (not in_correction_data_q.empty()):
                ready1 = True
            continue
        if not ready2:
            cd = in_correction_data_q.get()
            in_correction_data_q.task_done()
            # only set first time, then increases itself
            sampling_time_current = cd.center_time_prev
            # set every time
            phase1 = cd.center_phase_prev
            phase_slope = cd.phase_slope
            center_time_prev = cd.center_time_prev
            sampling_time_unit = cd.sampling_time_unit
            ready2 = True
            print('proc 1 worker sending start')
            continue

        # main loop protected by continues
        inc = inc_buf[read_idx]

        if time_current > center_time_prev:
            phase1 = phase1 + phase_slope * t_unit
            phase1 = phase1 % (2*np.pi)
            correction = np.exp(-1j * phase1)
            inc_corr = inc * correction

            if (
              (sampling_time_current >= (time_current - t_unit))
              and (sampling_time_current <= time_current)):
                time_left = sampling_time_current - (time_current - t_unit)
                interp = (
                    prev_inc_corr
                    + (inc_corr - prev_inc_corr) * time_left / t_unit)
                sampling_time_current += sampling_time_unit
                out_q.put(interp)

            prev_inc_corr = inc_corr
        time_current = time_current + t_unit

        # update ifg parameters
        if time_current > cd.center_time_observed:
            cd = in_correction_data_q.get()
            in_correction_data_q.task_done()
            phase1 = cd.center_phase_prev
            phase_slope = cd.phase_slope
            center_time_prev = cd.center_time_prev
            sampling_time_unit = cd.sampling_time_unit
            print('proc 1 worker update')

        if read_idx == buf_size3 - 1:
            read_idx = 0
        else:
            read_idx += 1


sig_q = Queue()
sig_h_q = Queue()
proc1_q = Queue()
ifg1_q = Queue()
ifg1_time_q = Queue()
correction_data1_q = Queue()
proc2_q = Queue()

n = 58

threading.Thread(
    target=partial(sig_in_worker, sig_q, n), daemon=True).start()
threading.Thread(
    target=partial(hilbert_worker, sig_q, sig_h_q), daemon=True).start()
threading.Thread(
    target=partial(trig_worker, sig_h_q, proc1_q, ifg1_q, ifg1_time_q),
    daemon=True).start()
threading.Thread(
    target=partial(measure_worker, ifg1_q, ifg1_time_q, correction_data1_q),
    daemon=True).start()
threading.Thread(
    target=partial(process_worker, proc1_q, proc2_q, correction_data1_q),
    daemon=True).start()

sig_v = []
for idx in tqdm(range(int(n*52450*0.95/2))):
    sig_v.append(proc2_q.get())
sig_v = np.array(sig_v)/0.95

plt.plot(np.real(sig_v))
plt.show()

# %%

fft_temp3 = np.abs(np.fft.fft(sig_v))
f_ifg5 = np.fft.fftfreq(sig_v.size, d=t_unit_real)

fft_temp = np.abs(np.fft.fft(E_measured[0:52450*n]))
f_ifg = np.fft.fftfreq(E_measured[0:52450*n].size, d=t_unit_real)

plotmask2 = (f_ifg5 > 6e6) & (f_ifg5 < 6.2e6)

plt.plot(f_ifg5[plotmask2]/2, fft_temp3[plotmask2], linewidth=0.2)
# plt.xlim([0.006e8, 0.008e8])
# plt.xlim([0.4e8, 0.55e8])
# plt.ylim([0, 400])
plt.show()

plotmask2 = (f_ifg5 > -0.0002e8) & (f_ifg5 < 0.0002e8)

plt.plot(f_ifg5[plotmask2]/2, fft_temp3[plotmask2], linewidth=0.2)
# plt.xlim([0.00075e8, 0.00085e8])
# plt.xlim([0.4e8, 0.55e8])
# plt.ylim([0, 400])
plt.show()

plotmask2 = (f_ifg5 > -0.001e8) & (f_ifg5 < 0.001e8)
plt.plot(f_ifg5[plotmask2]/2, fft_temp3[plotmask2], ':')
# plt.xlim([0.00075e8, 0.00085e8])
# plt.xlim([0.4e8, 0.55e8])
# plt.ylim([0, 400])
plt.show()

plotmask2 = (f_ifg5/2 > 0.3e8) & (f_ifg5/2 < 0.7e8)
plt.plot(f_ifg5[plotmask2]/2, fft_temp3[plotmask2], linewidth=0.2)
# plt.xlim([0.00075e8, 0.00085e8])
# plt.xlim([0.4e8, 0.55e8])
# plt.ylim([-200, 200])
plt.show()

plt.plot(f_ifg, fft_temp, linewidth=0.2)
plt.plot(f_ifg5/2, -fft_temp3, linewidth=0.2)
# plt.ylim([-5, 5])
plt.show()
# %%
