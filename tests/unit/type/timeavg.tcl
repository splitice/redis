start_server {
    tags {"timeavg"}
} {

    test {TAHITX - constant rate for one length} {
        # 3 each second, total of 60
		set f 0
		for {set i 0} {$i < 32} {incr i} {
			incr f
			assert_equal $f [r tahitx 1 1 [expr 1024+$i] mytahitx1]
			incr f
			assert_equal $f [r tahitx 1 1 [expr 1024+$i] mytahitx1]
			incr f
			assert_equal $f [r tahitx 1 1 [expr 1024+$i] mytahitx1]
		}
		
        assert_equal [r ttl mytahitx1] 32
        assert_equal [r tacalc 1056 mytahitx1] 96
    }
    
    test {TAHITX - 1 rate for one length} {
		set f 0
		for {set i 0} {$i < 32} {incr i} {
			incr f
			assert_equal $f [r tahitx 1 1 [expr 1024+$i] mytahitxa]
		}
		
        assert_equal [r tacalc 1056 mytahitxa] 31
    }
    
    test {TAHITX - 1 rate for 2 length} {
		set f 0
		for {set i 0} {$i < 32} {incr i} {
			incr f
			assert_equal $f [r tahitx 2 1 [expr 1024+$i] mytahitxb]
		}
		
        assert_equal [r tacalc 1056 mytahitxb] 30
    }
    
    test {TAHITX - 1 rate for 30 length} {
		set f 0
		for {set i 0} {$i < 32} {incr i} {
			incr f
			assert_equal $f [r tahitx 30 1 [expr 1024+$i] mytahitxc]
		}
		
        assert_equal [r tacalc 1056 mytahitxc] 32
    }
	
	test {TAHITX - real like times repeating} {
		set f 0
		for {set i 0} {$i < 32} {incr i} {
			incr f
			r tahitx 30 1 [expr 1486331402+$i] mytahitxc
			r tahitx 30 1 [expr 1486331402+$i] mytahitxc
			r tahitx 30 2 [expr 1486331402+$i] mytahitxc
		}
		
        assert_equal [r tacalc 1486331434 mytahitxc] 128
    }
    
    test {TAHITX - real like times} {
		set f 0
		for {set i 0} {$i < 32} {incr i} {
			incr f
			assert_equal $f [r tahitx 30 1 [expr 1486331402+$i] mytahitxc]
		}
		
        assert_equal [r tacalc 1486331434 mytahitxc] 32
    }

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
		
        assert_equal [r tacalc 1055 mytahit1] 93
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
			r tahit 1 1 [expr 1024+64+$i] mytahit5
			set last [r tahit 1 1 [expr 1024+64+$i] mytahit5]
		}
		
        assert_equal 96 $last
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
			set res_hit [r tahit 1 1 [expr 1024+($i*4)] mytahit11]
		}
		
		set res [r tacalc [expr 1024+(32*4)-1] mytahit11]
		assert_equal $res_hit 8
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
