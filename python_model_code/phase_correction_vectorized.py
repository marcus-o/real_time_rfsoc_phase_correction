# %% 2024 Marcus Ossiander TU Graz
# model code implementing the goal
# phase correction algorithm in a python way
# you need test data (load in lines 16-23) to run the code 

import numpy as np
import matplotlib.pyplot as plt
from tqdm import tqdm
import h5py

from scipy.interpolate import PchipInterpolator
from scipy.signal import find_peaks

# import os
# %% load data
# folder = os.path.dirname(os.path.realpath(__file__)) + '/'
folder = 'fpga/python_model_code/'
filename = 'acetylene_test_data.hdf5'

with h5py.File(folder + filename, 'r') as hdf5_file:
    t_unit = float(hdf5_file['time_step_s'][0])
    E_measured = np.array(hdf5_file['photodiode_signal'][:])
    t_whole = np.arange(E_measured.size) * t_unit

rep_rate = 1e9

# hilbert transform and low pass filtering
f_ifg_full = np.fft.fftfreq(t_whole.size, d=t_unit)
spec_ifg = np.fft.fft(E_measured)
# information-bearing part and Hilbert transform
spec_ifg[f_ifg_full < 1e6] = 0
spec_ifg[f_ifg_full > 0.99*rep_rate/2] = 0
E_analytical = np.fft.ifft(spec_ifg)

# find interferograms, this is the most important step
delta_t = 25000
peaks, _ = find_peaks(
    np.abs(E_analytical), height=0.02, distance=2*delta_t)
peaks = peaks[(peaks > delta_t) & (peaks < E_analytical.size - delta_t)]

# plot first couple peaks
plt.plot(t_whole[:delta_t*14], np.abs(E_analytical)[:delta_t*14])
plt.plot(t_whole[peaks[:7]], np.abs(E_analytical)[peaks[:7]], 'o')
plt.xlim([np.min(t_whole[:delta_t*10]), np.max(t_whole[:delta_t*10])])
plt.show()

# %% measure frequency and time of the first interferogram
# interval lengths to cut around peak
size_ifg = 15
size_spec = 200

p = peaks[0]
# cut around peak
ifg = E_analytical[p-size_ifg:p+size_ifg]
t_ifg = t_whole[p-size_ifg:p+size_ifg]

# find exact center of first ifg
center_time_observed = (
    np.sum(np.abs(ifg)**2 * t_ifg) / np.sum(np.abs(ifg)**2))
shift = int(center_time_observed // t_unit)

# frequency mask for central frequency
f_ifg = np.fft.fftfreq(size_spec*2, d=t_unit)
freq_mask = (f_ifg > 91e6) & (f_ifg < 291e6)
# measure frequency of first interferogram
spec_ifg = np.fft.fft(
    E_analytical[shift-size_spec:shift+size_spec])
center_freq_observed = (
    np.sum(np.abs(spec_ifg[freq_mask])**4 * f_ifg[freq_mask])
    / np.sum(np.abs(spec_ifg[freq_mask])**4))

# subtract frequency of the first interferogram
phase1 = np.arange(t_whole.size) * center_freq_observed * 2*np.pi * t_unit
correction = np.exp(-1j * phase1)
E_no_carrier = E_analytical * correction
del phase1, correction

# %% measure phase and time of each interferogram
center_time_observeds = np.zeros(peaks.size)
center_phase_observeds = np.zeros(peaks.size)

for idx, p in enumerate(tqdm(peaks)):

    # cut around peak
    ifg = E_no_carrier[p-size_ifg:p+size_ifg]
    t_ifg = t_whole[p-size_ifg:p+size_ifg]

    # time correction measure
    center_time_observeds[idx] = (
        np.sum(np.abs(ifg)**2 * t_ifg) / np.sum(np.abs(ifg)**2))
    shift = int(center_time_observeds[idx] // t_unit)

    # phase correction measure
    center_phase_observeds[idx] = np.angle(E_no_carrier[shift])

# %% correct for the phase difference between interferograms
spl1 = PchipInterpolator(
    center_time_observeds, np.unwrap(center_phase_observeds), extrapolate=True)
phase1 = spl1(t_whole)
correction = np.exp(-1j * phase1)
E_phase_corrected = E_no_carrier * correction
del phase1, correction

# %% resample time grid to correct detuning fluctuations
t_rep = (t_whole[peaks[-1]] - t_whole[peaks[0]]) / (peaks.size-1)
center_time_observeds_optimal = np.arange(0, peaks.size, 1)*t_rep

spl1 = PchipInterpolator(
    center_time_observeds,
    center_time_observeds_optimal,
    extrapolate=True)

t_whole_correct = spl1(t_whole)

E_resampled = PchipInterpolator(
    t_whole_correct,
    E_phase_corrected,
    extrapolate=True)(t_whole)

E_resampled[t_whole > np.max(t_whole_correct)] = 0
E_resampled[t_whole < np.min(t_whole_correct)] = 0
del t_whole_correct

# %% only plotting from here
spec_no_carrier = np.abs(np.fft.fft(E_no_carrier[::2]))*2
spec_resampled = np.abs(np.fft.fft(E_resampled[::2]))*2
f_ifg_full = np.fft.fftfreq(t_whole.size//2, d=t_unit*2)

freq_mask_plot = (f_ifg_full > 6e6) & (f_ifg_full < 6.2e6)

plt.plot(
    f_ifg_full[freq_mask_plot], spec_no_carrier[freq_mask_plot],
    linewidth=0.2)
plt.plot(
    f_ifg_full[freq_mask_plot], spec_resampled[freq_mask_plot],
    linewidth=0.2)
# plt.xlim([0.006e8, 0.008e8])
# plt.xlim([0.4e8, 0.55e8])
plt.ylim([0, 400])
plt.show()

freq_mask_plot = (f_ifg_full > 0.000761e8) & (f_ifg_full < 0.000767e8)

plt.plot(
    f_ifg_full[freq_mask_plot], spec_no_carrier[freq_mask_plot],
    linewidth=0.2)
plt.plot(
    f_ifg_full[freq_mask_plot], spec_resampled[freq_mask_plot],
    'x-', linewidth=0.2)
# plt.xlim([0.00075e8, 0.00085e8])
# plt.xlim([0.4e8, 0.55e8])
plt.ylim([0, 350])
plt.show()

freq_mask_plot = (f_ifg_full > -0.0002e8) & (f_ifg_full < 0.0002e8)

plt.plot(
    f_ifg_full[freq_mask_plot], spec_no_carrier[freq_mask_plot],
    linewidth=0.2)
plt.plot(
    f_ifg_full[freq_mask_plot], spec_resampled[freq_mask_plot],
    linewidth=0.2)
# plt.xlim([0.00075e8, 0.00085e8])
# plt.xlim([0.4e8, 0.55e8])
plt.ylim([0, 400])
plt.show()

freq_mask_plot = (f_ifg_full > -0.001e8) & (f_ifg_full < 0.001e8)
plt.plot(f_ifg_full[freq_mask_plot], spec_no_carrier[freq_mask_plot])
plt.plot(f_ifg_full[freq_mask_plot], spec_resampled[freq_mask_plot], ':')
# plt.xlim([0.00075e8, 0.00085e8])
# plt.xlim([0.4e8, 0.55e8])
plt.ylim([0, 400])
plt.show()

freq_mask_plot = (f_ifg_full > 0.000188e8) & (f_ifg_full < 0.000194e8)

plt.plot(
    f_ifg_full[freq_mask_plot], spec_no_carrier[freq_mask_plot],
    linewidth=0.2)
plt.plot(
    f_ifg_full[freq_mask_plot], spec_resampled[freq_mask_plot], 'x-',
    linewidth=0.2)
# plt.xlim([0.00075e8, 0.00085e8])
# plt.xlim([0.4e8, 0.55e8])
plt.ylim([0, 400])
plt.show()

freq_mask_plot = (f_ifg_full > 0.3e8) & (f_ifg_full < 0.7e8)
plt.plot(
    f_ifg_full[freq_mask_plot], spec_no_carrier[freq_mask_plot],
    linewidth=0.2)
plt.plot(
    f_ifg_full[freq_mask_plot], -spec_resampled[freq_mask_plot],
    linewidth=0.2)
# plt.xlim([0.00075e8, 0.00085e8])
# plt.xlim([0.4e8, 0.55e8])
plt.ylim([-200, 200])
plt.show()

plt.plot(f_ifg_full, spec_no_carrier*0.95, linewidth=0.2)
plt.plot(f_ifg_full, -spec_resampled*0.95, linewidth=0.2)
plt.show()

# %%
