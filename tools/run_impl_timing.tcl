open_project Zynq_GPGPU_Core.xpr

reset_run synth_1
reset_run impl_1

set run [get_runs impl_1]
set_property strategy Performance_NetDelay_high $run
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true $run
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveExplore $run

launch_runs impl_1 -to_step write_bitstream -jobs 12
wait_on_run impl_1

open_run impl_1
report_timing_summary -delay_type max -report_unconstrained -max_paths 10 -file impl_timing_summary.rpt
report_utilization -file impl_utilization_summary.rpt

close_project
exit
