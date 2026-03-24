open_project Zynq_GPGPU_Core.xpr

launch_runs impl_1 -to_step write_bitstream -jobs 12
wait_on_run impl_1

close_project
exit
