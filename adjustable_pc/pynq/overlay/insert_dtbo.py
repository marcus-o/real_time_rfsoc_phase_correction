# if no *.dtbo is here, install
# sudo apt-get install -y device-tree-compiler
# and run
# dtc -I dts -O dtb -o ddr4.dtbo ddr4.dts -q

# in any case, add this line (adjust the path):
# python3 /home/xilinx/jupyter_notebooks/marcus/pl_access/overl/insert_dtbo.py
# to the end of /etc/profile.d/pynq_venv.sh
# and restart the rfsoc4x2

import pynq
dts = pynq.DeviceTreeSegment('/home/xilinx/jupyter_notebooks/adjustable_pc/overlay/ddr4.dtbo')
if not dts.is_dtbo_applied():
    print('applying dtbo')
    dts.insert()