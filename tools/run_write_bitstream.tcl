open_project Zynq_GPGPU_Core.xpr

set run [get_runs impl_1]
set_property strategy Performance_NetDelay_high $run
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true $run
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveExplore $run

launch_runs impl_1 -to_step write_bitstream -jobs 12
wait_on_run impl_1

close_project
exit
