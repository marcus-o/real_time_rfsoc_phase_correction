# %%

import numpy as np
import matplotlib.pyplot as plt


folder = 'C:/FPGA/real_time_rfsoc_phase_correction/adjustable_pc_high_speed/vitis_hls/phase_c_dr/test_scripts/'
# folder = 'C:/FPGA/real_time_rfsoc_phase_correction/adjustable_pc_2ch/vitis_hls/phase_c_dr/test_scripts/'

# %%
dat = np.loadtxt(folder + 'out_ref_ch_avg.txt')
dat = dat[0::2] + 1j*dat[1::2]

plt.plot(np.real(dat), '.-')
plt.plot(np.imag(dat), '.-')
plt.xlim([int(dat.size/2)-25, int(dat.size/2)+25])
plt.show()

plt.plot(np.real(dat))
plt.plot(np.imag(dat))
plt.show()

plt.plot(np.abs(np.fft.fftshift(np.fft.fft(dat))))
plt.show()

plt.plot(np.abs(np.fft.fftshift(np.fft.fft(dat))))
plt.xlim([8000, 10000])
plt.show()
print(np.max(np.abs(np.fft.fftshift(np.fft.fft(dat)))))

# %%
dat = np.loadtxt(folder + 'out_orig_corrected.txt')
dat = dat[0::2] + 1j*dat[1::2]
dat2 = dat
plt.plot(np.real(dat), '.-')
plt.plot(np.imag(dat), '.-')
plt.xlim([6850, 7000])
plt.show()

plt.plot(np.real(dat), '.-')
plt.plot(np.imag(dat), '.-')
plt.xlim([0, 5000])
plt.show()

plt.plot(np.real(dat))
plt.plot(np.imag(dat))
plt.show()

plt.plot(np.abs(np.fft.fftshift(np.fft.fft(dat))))
plt.show()

plt.plot(np.abs(np.fft.fftshift(np.fft.fft(dat))), 'x-')
plt.xlim([400000, 400500])
plt.show()
print(np.max(np.abs(np.fft.fftshift(np.fft.fft(dat)))))

# %%
dat = np.loadtxt(folder + 'out_orig.txt')
dat = dat[0::2] + 1j*dat[1::2]

plt.plot(np.real(dat), '.-')
plt.plot(np.imag(dat), '.-')
plt.xlim([16150, 16250])
plt.show()

plt.plot(np.real(dat))
plt.plot(np.imag(dat))

#plt.plot(np.arange(dat2.size)/0.95, np.real(dat2)-4000, '.-')
#plt.plot(np.arange(dat2.size)/0.95, np.imag(dat2)-4000, '.-')

plt.show()

plt.plot(np.abs(np.fft.fftshift(np.fft.fft(dat))))
plt.show()

plt.plot(np.abs(np.fft.fftshift(np.fft.fft(dat))), 'x-')
plt.xlim([400000, 400500])
plt.show()
print(np.max(np.abs(np.fft.fftshift(np.fft.fft(dat)))))

# %%
dat = np.loadtxt(folder + 'out_log.txt')
delta_time_exact = dat[0::4]
phase_pi = dat[1::4]
center_freq = dat[2::4]
phase_change_pi = dat[3::4]

plt.plot(delta_time_exact, '-x')
plt.show()

plt.plot(phase_pi, '-x')
plt.show()

plt.plot(phase_change_pi, '-x')
plt.ylim([-2, 2])
plt.show()

plt.plot(center_freq, '-x')
plt.show()

print(np.max(phase_change_pi))

# %%
