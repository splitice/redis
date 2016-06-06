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
		for {set i 0} {$i < 10} {incr i} {
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
		
        assert_equal 60 [r tacalc 1 159 mytahit5]
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
		
        assert_equal 60 [r tacalc 1 149 mytahit6]
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
		
        assert_equal 30 [r tacalc 1 179 mytahit7]
	}
	
    test {TAHIT - should equal TACALC} {
		set f 0
		for {set i 0} {$i < 20} {incr i} {
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit8]
			assert_equal $f [r tacalc 1 [expr 100+$i] mytahit8]
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit8]
			assert_equal $f [r tacalc 1 [expr 100+$i] mytahit8]
			incr f
			assert_equal $f [r tahit 1 1 [expr 100+$i] mytahit8]
			assert_equal $f [r tacalc 1 [expr 100+$i] mytahit8]
		}
		
        assert_equal 60 [r tacalc 1 119 mytahit1]
    }
	
    test {TAHIT - rare incr} {
		assert_equal 1 [r tahit 1 1 [expr 100] mytahit9]
		assert_equal 1 [r tacalc 1 [expr 100] mytahit9]
		assert_equal 1 [r tahit 1 1 [expr 200] mytahit9]
		assert_equal 1 [r tacalc 1 [expr 200] mytahit9]
		assert_equal 1 [r tahit 1 1 [expr 300] mytahit9]
		assert_equal 1 [r tacalc 1 [expr 300] mytahit9]
    }
	
    test {TAHIT - backwards time} {
		assert_equal 1 [r tahit 1 1 [expr 100] mytahit10]
		assert_equal 1 [r tacalc 1 [expr 100] mytahit10]
		assert_equal 2 [r tahit 1 1 [expr 99] mytahit10]
		assert_equal 2 [r tacalc 1 [expr 99] mytahit10]
		assert_equal 3 [r tahit 1 1 [expr 100] mytahit10]
		assert_equal 3 [r tacalc 1 [expr 100] mytahit10]
    }
	
	test {TAHIT - constant slow rate} {
		set f 0
		set res_hit 0
		for {set i 0} {$i < 20} {incr i} {
			set res_hit [r tahit 1 1 [expr 100+($i*3)] mytahit11]
		}
		
		set res [r tacalc 1 160 mytahit11]
		puts $res
		puts $res_hit
        assert [expr $res < 7 && $res > 6]
		assert_equal $res $res_hit
	}
	
	test {TAHIT - constant long} {
		set f 0
		for {set i 0} {$i < 200} {incr i} {
			r tahit 1 1 [expr 100+$i] mytahit12
		}
		
		assert_equal 20 [r tacalc 1 199 mytahit12]
	}
	
	test {TAHIT - constant 1} {
		set f 0
		for {set i 0} {$i < 200} {incr i} {
			assert_equal 1 [r tahit 1 1 [expr 100+($i * 20)]] mytahit13
			assert_equal 1 [r tacalc 1 [expr 100+($i * 20)]] mytahit13
		}
		
		assert_equal 20 [r tacalc 1 300 mytahit13]
	}
}
