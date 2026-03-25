open_project Zynq_GPGPU_Core.xpr

set run [get_runs impl_1]
reset_run $run

set_property strategy Performance_NetDelay_high $run
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.IS_ENABLED true $run
set_property STEPS.POST_ROUTE_PHYS_OPT_DESIGN.ARGS.DIRECTIVE AggressiveExplore $run

puts "Running combo: Performance_NetDelay_high + post-route phys_opt AggressiveExplore"
launch_runs $run -to_step write_bitstream -jobs 12
wait_on_run $run

set wns [get_property STATS.WNS $run]
set tns [get_property STATS.TNS $run]
set whs [get_property STATS.WHS $run]
set ths [get_property STATS.THS $run]
set status [get_property STATUS $run]

puts [format "RESULT combo=NetDelayHigh_PostPhysAgg  WNS=%.6f  TNS=%.6f  WHS=%.6f  THS=%.6f  STATUS=%s" \
    $wns $tns $whs $ths $status]

set fh [open timing_strategy_sweep2.csv a]
puts $fh [format "%s,%.6f,%.6f,%.6f,%.6f,\"%s\"" \
    "NetDelayHigh_PostPhysAgg" $wns $tns $whs $ths $status]
close $fh

close_project
exit
