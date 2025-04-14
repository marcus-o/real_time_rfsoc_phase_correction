# instructions to access the pl ddr ram from pynq

# 0. if no *.dtbo is here, install
# sudo apt-get install -y device-tree-compiler
# and run
# dtc -I dts -O dtb -o ddr4.dtbo ddr4.dts -q

# in any case:

# 1. add this line (adjust the path to point to this file):
# python3 /home/xilinx/jupyter_notebooks/marcus/pl_access/overl/insert_dtbo.py
# to the end of /etc/profile.d/pynq_venv.sh

# 2. adjust the path in this file to point at the ddr4.dtbo file:

# 3. restart the rfsoc4x2

path = '/home/xilinx/jupyter_notebooks/adjustable_pc/overlay/ddr4.dtbo'

import pynq
dts = pynq.DeviceTreeSegment(path)
if not dts.is_dtbo_applied():
    print('applying dtbo')
    dts.insert()