from config import *
import time

from litescope.host.driver import LiteScopeLADriver
la = LiteScopeLADriver(wb.regs, "la", debug=True)

wb.open()
regs = wb.regs
###
regs.ethphy_crg_reset.write(1)
regs.ethphy_crg_reset.write(0)
time.sleep(5)
regs.bist_generator_src_port.write(0x1234)
regs.bist_generator_dst_port.write(0x5678)
regs.bist_generator_ip_address.write(0x12345678)
regs.bist_generator_length.write(64)

conditions = {}
conditions = {
	"udpip_core_mac_tx_cdc_sink_stb"	: 1
}
la.configure_term(port=0, cond=conditions)
la.configure_sum("term")
# Run Logic Analyzer
la.run(offset=64, length=1024)

regs.bist_generator_start.write(1)

while not la.done():
	pass

la.upload()
la.save("dump.vcd")

###
wb.close()
