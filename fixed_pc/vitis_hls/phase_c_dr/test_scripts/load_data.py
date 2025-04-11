# %%

import numpy as np
import matplotlib.pyplot as plt

dat = np.loadtxt('C:/Users/Labor/FPGA/vivado_2022_1/real_time_rfsoc_phase_correction/vitis_hls/phase_c_dr/test_scripts/output.txt')
dat = dat[0::2] + 1j*dat[1::2]

plt.plot(np.real(dat))
plt.plot(np.imag(dat))
plt.xlim([6000, 8000])
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
