start_server {
    tags {"timeavg"}
} {
    test {TAHIT - constant rate for one length} {
        # 3 each second, total of 60
		set f 0
		for {set i 0} {$i < 32} {incr i} {
			incr f
			assert_equal $f [r tahit 1 1 [expr 1024+$i] mytahit1]
			incr f
			assert_equal $f [r tahit 1 1 [expr 1024+$i] mytahit1]
			incr f
			assert_equal $f [r tahit 1 1 [expr 1024+$i] mytahit1]
		}
		
        assert_equal 96 [r tacalc 1055 mytahit1]
    }
	
	test {TAHIT - varied rate for one length} {
        # 4,2,4,2.... expected average of 3/s (60)
		for {set i 0} {$i < 32} {incr i} {
			if {fmod($i,2) == 0} {
				r tahit 1 1 [expr 1024+$i] mytahit2
				r tahit 1 1 [expr 1024+$i] mytahit2
			}
			r tahit 1 1 [expr 1024+$i] mytahit2
			r tahit 1 1 [expr 1024+$i] mytahit2
		}
		
        assert_equal 96 [r tacalc 1055 mytahit2]
    }
	
	test {TAHIT - constant rate for 2 length} {
		# 3 each second, total of 60
		set f 0
		for {set i 0} {$i < 32} {incr i} {
			incr f
			assert_equal $f [r tahit 1 1 [expr 1024+$i] mytahit4]
			incr f
			assert_equal $f [r tahit 1 1 [expr 1024+$i] mytahit4]
			incr f
			assert_equal $f [r tahit 1 1 [expr 1024+$i] mytahit4]
		}
		for {set i 0} {$i < 32} {incr i} {
			r tahit 1 1 [expr 1024+32+$i] mytahit4
			r tahit 1 1 [expr 1024+32+$i] mytahit4
			r tahit 1 1 [expr 1024+32+$i] mytahit4
		}
		
        assert_equal 96 [r tacalc [expr 1024+32+31] mytahit4]
	}
	
	test {TAHIT - constant rate for 2 length with gap (aligned)} {
		# 3 each second, total of 60
		set f 0
		for {set i 0} {$i < 32} {incr i} {
			incr f
			assert_equal $f [r tahit 1 1 [expr 1024+$i] mytahit5]
			incr f
			assert_equal $f [r tahit 1 1 [expr 1024+$i] mytahit5]
			incr f
			assert_equal $f [r tahit 1 1 [expr 1024+$i] mytahit5]
		}
		for {set i 0} {$i < 32} {incr i} {
			r tahit 1 1 [expr 1024+64+$i] mytahit5
			r tahit 1 1 [expr 1024+64$i] mytahit5
			r tahit 1 1 [expr 1024+64$i] mytahit5
		}
		
        assert_equal 96 [r tacalc [expr 1024+64+31] mytahit5]
	}
	
    test {TAHIT - should equal TACALC} {
		set f 0
		for {set i 0} {$i < 32} {incr i} {
			incr f
			assert_equal $f [r tahit 1 1 [expr 1024+$i] mytahit8]
			assert_equal $f [r tacalc [expr 1024+$i] mytahit8]
			incr f
			assert_equal $f [r tahit 1 1 [expr 1024+$i] mytahit8]
			assert_equal $f [r tacalc [expr 1024+$i] mytahit8]
			incr f
			assert_equal $f [r tahit 1 1 [expr 1024+$i] mytahit8]
			assert_equal $f [r tacalc [expr 1024+$i] mytahit8]
		}
		
        assert_equal 96 [r tacalc [expr 1024+31] mytahit1]
    }
	
    test {TAHIT - rare incr} {
		assert_equal 1 [r tahit 1 1 128 mytahit9]
		assert_equal 1 [r tacalc 128 mytahit9]
		assert_equal 1 [r tahit 1 1 256 mytahit9]
		assert_equal 1 [r tacalc 256 mytahit9]
		assert_equal 1 [r tahit 1 1 512 mytahit9]
		assert_equal 1 [r tacalc 512 mytahit9]
    }
	
    test {TAHIT - backwards time} {
		assert_equal 1 [r tahit 1 1 128 mytahit10]
		assert_equal 1 [r tacalc 128 mytahit10]
		assert_equal 2 [r tahit 1 1 127 mytahit10]
		assert_equal 2 [r tacalc 127 mytahit10]
		assert_equal 3 [r tahit 1 1 128 mytahit10]
		assert_equal 3 [r tacalc 128 mytahit10]
    }
	
	test {TAHIT - constant slow rate} {
		set f 0
		set res_hit 0
		for {set i 0} {$i <= 32} {incr i} {
			set res_hit [r tahit 1 1 [expr 1024+($i*3)] mytahit11]
		}
		
		set res [r tacalc [expr 1024+(32*3)-1] mytahit11]
        assert [expr $res <= 7 && $res >= 6]
		assert_equal $res $res_hit
	}
	
	test {TAHIT - constant long} {
		set f 0
		for {set i 0} {$i < 1024} {incr i} {
			r tahit 1 1 [expr 1024+$i] mytahit12
		}
		
		assert_equal 32 [r tacalc 2047 mytahit12]
	}
	
	test {TAHIT - constant 1} {
		set f 0
		for {set i 0} {$i < 1024} {incr i} {
			assert_equal 1 [r tahit 1 1 [expr 1024+($i * 32)] mytahit13]
			assert_equal 1 [r tacalc [expr 1024+($i * 32)] mytahit13]
		}
		
		assert_equal 1 [r tacalc 2047 mytahit13]
	}
}
