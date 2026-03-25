open_project Zynq_GPGPU_Core.xpr

set synth_run [get_runs synth_1]
set synth_status [get_property STATUS $synth_run]
if {![string match "*Complete*" $synth_status]} {
    puts "synth_1 not complete (STATUS=$synth_status), launching synth_1..."
    launch_runs synth_1 -jobs 12
    wait_on_run synth_1
}

set impl_run [get_runs impl_1]
set strategies {
    Performance_Explore
    Performance_ExplorePostRoutePhysOpt
    Performance_NetDelay_high
    Performance_RefinePlacement
    Performance_ExtraTimingOpt
    Flow_RunPostRoutePhysOpt
}

set out_csv "timing_strategy_sweep.csv"
set fh [open $out_csv w]
puts $fh "strategy,wns,tns,whs,ths,status"

set best_strategy ""
set best_wns -9999.0
set best_tns -9999.0

foreach strat $strategies {
    puts "Running impl_1 with strategy=$strat"

    reset_run $impl_run
    set_property strategy $strat $impl_run

    launch_runs $impl_run -to_step route_design -jobs 12
    wait_on_run $impl_run

    set run_status [get_property STATUS $impl_run]
    set wns [get_property STATS.WNS $impl_run]
    set tns [get_property STATS.TNS $impl_run]
    set whs [get_property STATS.WHS $impl_run]
    set ths [get_property STATS.THS $impl_run]

    puts [format "RESULT strategy=%s  WNS=%.3f  TNS=%.3f  WHS=%.3f  THS=%.3f  STATUS=%s" \
        $strat $wns $tns $whs $ths $run_status]
    puts $fh [format "%s,%.6f,%.6f,%.6f,%.6f,\"%s\"" \
        $strat $wns $tns $whs $ths $run_status]
    flush $fh

    if {$wns > $best_wns || ($wns == $best_wns && $tns > $best_tns)} {
        set best_wns $wns
        set best_tns $tns
        set best_strategy $strat
    }

    if {$wns >= 0.0 && $tns >= 0.0 && $whs >= 0.0 && $ths >= 0.0} {
        puts "Timing closure achieved with strategy=$strat"
        break
    }
}

close $fh

puts [format "Best strategy=%s  WNS=%.3f  TNS=%.3f" $best_strategy $best_wns $best_tns]
puts "Sweep summary saved to timing_strategy_sweep.csv"

close_project
exit
