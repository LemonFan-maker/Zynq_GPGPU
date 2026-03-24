# External reset port constraint for gpu_system_wrapper
# Fixes DRC UCIO-1 / NSTD-1 on reset_rtl_0.
# NOTE: Pin W6 is selected based on current implementation IO placement report.

set_property PACKAGE_PIN W6 [get_ports {reset_rtl_0}]
set_property IOSTANDARD LVCMOS18 [get_ports {reset_rtl_0}]

# Keep reset deasserted when pin is left floating.
set_property PULLDOWN true [get_ports {reset_rtl_0}]
