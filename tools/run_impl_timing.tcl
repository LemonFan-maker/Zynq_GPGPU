open_project Zynq_GPGPU_Core.xpr

reset_run synth_1
reset_run impl_1

launch_runs impl_1 -to_step route_design -jobs 12
wait_on_run impl_1

open_run impl_1
report_timing_summary -delay_type max -report_unconstrained -max_paths 10 -file impl_timing_summary.rpt
report_utilization -file impl_utilization_summary.rpt

close_project
exit
