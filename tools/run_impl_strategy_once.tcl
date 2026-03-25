if {$argc < 1} {
    puts "Usage: vivado -mode batch -source tools/run_impl_strategy_once.tcl -tclargs <strategy> ?<csv_path>?"
    exit 1
}

set strategy_name [lindex $argv 0]
set csv_path ""
if {$argc >= 2} {
    set csv_path [lindex $argv 1]
}

open_project Zynq_GPGPU_Core.xpr

set impl_run [get_runs impl_1]
set valid_strategies [list_property_value strategy $impl_run]
if {[lsearch -exact $valid_strategies $strategy_name] < 0} {
    puts "ERROR: invalid strategy '$strategy_name'"
    puts "Valid strategies: $valid_strategies"
    close_project
    exit 2
}

reset_run $impl_run
set_property strategy $strategy_name $impl_run

puts "Running impl_1 with strategy=$strategy_name"
launch_runs $impl_run -to_step route_design -jobs 12
wait_on_run $impl_run

set run_status [get_property STATUS $impl_run]
set wns [get_property STATS.WNS $impl_run]
set tns [get_property STATS.TNS $impl_run]
set whs [get_property STATS.WHS $impl_run]
set ths [get_property STATS.THS $impl_run]

puts [format "RESULT strategy=%s  WNS=%.6f  TNS=%.6f  WHS=%.6f  THS=%.6f  STATUS=%s" \
    $strategy_name $wns $tns $whs $ths $run_status]

if {$csv_path ne ""} {
    set exists [file exists $csv_path]
    set fh [open $csv_path a]
    if {!$exists} {
        puts $fh "strategy,wns,tns,whs,ths,status"
    }
    puts $fh [format "%s,%.6f,%.6f,%.6f,%.6f,\"%s\"" \
        $strategy_name $wns $tns $whs $ths $run_status]
    close $fh
}

close_project
exit
