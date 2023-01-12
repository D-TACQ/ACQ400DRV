#!/usr/local/bin/expect
# for full block set export SLOWMON_COUNT=0 in /mnt/local/sysconfig/acq400.sh
# for non standard block size export SLOWMON_BS in /mnt/local/sysconfig/acq400.sh

log_user 0

set COUNT 1
if { [info exists ::env(SLOWMON_COUNT) ] } {
	set COUNT $::env(SLOWMON_COUNT)
}

set BS 4096
if { [info exists ::env(SLOWMON_BS) ] } {
	set BS $::env(SLOWMON_BS)
}

if { $COUNT == 0 } {
	set BL [exec get.site 0 bufferlen]
        set BLC [expr $BL / $BS]
	set COUNT_CMD "count=$BLC"
} else {
	set COUNT_CMD "count=$COUNT"
}



spawn -open [open "/dev/acq400.0.bqf" r]

while { 1 } {
    expect {
        -re "^(\[0-9\]*)\n" {
            set blk $expect_out(1,string)
#	    puts $blk;flush stdout
	    exec >@stdout 2>/dev/null \
		dd if=/dev/acq400.0.hb/$blk bs=$BS $COUNT_CMD
#	    puts "dd if=/dev/acq400.0.hb/$blk bs=$BS $COUNT_CMD"
        }
    }
}



