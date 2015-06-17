start_server {
    tags {"timeavg"}
} {
    test {TAHIT - constant rate for one length} {
        # 3 each second, total of 60
		set f 0
		for {set i 0} {$i < 20} {incr i} {
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit1]
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit1]
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit1]
		}
		
        assert_equal 60 [r tacalc 1 119 mytahit1]
    }
	
	test {TAHIT - varied rate for one length} {
        # 4,2,4,2.... expected average of 3/s (60)
		for {set i 0} {$i < 20} {incr i} {
			if {fmod($i,2) == 0} {
				r tahit 1 1 [expr 100+$i] mytahit2
				r tahit 1 1 [expr 100+$i] mytahit2
			}
			r tahit 1 1 [expr 100+$i] mytahit2
			r tahit 1 1 [expr 100+$i] mytahit2
		}
		
        assert_equal 60 [r tacalc 1 119 mytahit2]
    }
	
	test {TAHIT - constant rate for 1.5 length} {
		# 3 each second, total of 60
		set f 0
		for {set i 0} {$i < 20} {incr i} {
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit3]
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit3]
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit3]
		}
		for {set i 0} {$i < 20} {incr i} {
			r tahit 1 1 [expr 120+$i] mytahit3
			r tahit 1 1 [expr 120+$i] mytahit3
			r tahit 1 1 [expr 120+$i] mytahit3
		}
		
        assert_equal 60 [r tacalc 1 129 mytahit3]
	}
	
	test {TAHIT - constant rate for 2 length} {
		# 3 each second, total of 60
		set f 0
		for {set i 0} {$i < 20} {incr i} {
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit4]
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit4]
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit4]
		}
		for {set i 0} {$i < 20} {incr i} {
			r tahit 1 1 [expr 120+$i] mytahit4
			r tahit 1 1 [expr 120+$i] mytahit4
			r tahit 1 1 [expr 120+$i] mytahit4
		}
		
        assert_equal 60 [r tacalc 1 139 mytahit4]
	}
	
	test {TAHIT - constant rate for 2 length with gap (aligned)} {
		# 3 each second, total of 60
		set f 0
		for {set i 0} {$i < 20} {incr i} {
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit5]
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit5]
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit5]
		}
		for {set i 0} {$i < 20} {incr i} {
			r tahit 1 1 [expr 140+$i] mytahit5
			r tahit 1 1 [expr 140+$i] mytahit5
			r tahit 1 1 [expr 140+$i] mytahit5
		}
		
        assert_equal 60 [r tacalc 1 169 mytahit5]
	}
	
	test {TAHIT - constant rate for 2 length with gap (not aligned, less than buckets)} {
		# 3 each second, total of 60
		set f 0
		for {set i 0} {$i < 20} {incr i} {
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit6]
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit6]
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit6]
		}
		for {set i 0} {$i < 20} {incr i} {
			r tahit 1 1 [expr 130+$i] mytahit6
			r tahit 1 1 [expr 130+$i] mytahit6
			r tahit 1 1 [expr 130+$i] mytahit6
		}
		
        assert_equal 60 [r tacalc 1 139 mytahit6]
	}
	
	test {TAHIT - constant rate for 2 length with gap (not aligned, more than buckets)} {
		# 3 each second, total of 60
		set f 0
		for {set i 0} {$i < 20} {incr i} {
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit7]
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit7]
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit7]
		}
		for {set i 0} {$i < 20} {incr i} {
			r tahit 1 1 [expr 150+$i] mytahit7
			r tahit 1 1 [expr 150+$i] mytahit7
			r tahit 1 1 [expr 150+$i] mytahit7
		}
		
        assert_equal 60 [r tacalc 1 139 mytahit7]
	}
}
