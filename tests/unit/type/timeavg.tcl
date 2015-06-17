start_server {
    tags {"timeavg"}
} {
    test {TAHIT - constant rate for one length} {
        # 3 each second, total of 60
		for {set i 0} {$i < 20} {incr i} {
			assert {[r tahit 1 1 [expr 100+$i] mytahit1] eq {OK}}
			assert {[r tahit 1 1 [expr 100+$i] mytahit1] eq {OK}}
			assert {[r tahit 1 1 [expr 100+$i] mytahit1] eq {OK}}
		}
		
        assert_equal 60 [r tacalc 1 1019 mytahit1]
    }
	
	test {TAHIT - varied rate for one length} {
        # 4,2,4,2.... expected average of 3/s (60)
		for {set i 0} {$i < 20} {incr i} {
			if {fmod($i,2) == 0} {
				assert {[r tahit 1 1 [expr 100+$i] mytahit2] eq {OK}}
				assert {[r tahit 1 1 [expr 100+$i] mytahit2] eq {OK}}
				assert {[r tahit 1 1 [expr 100+$i] mytahit2] eq {OK}}
				assert {[r tahit 1 1 [expr 100+$i] mytahit2] eq {OK}}
			}
			assert {[r tahit 1 1 [expr 100+$i] mytahit2] eq {OK}}
			assert {[r tahit 1 1 [expr 100+$i] mytahit2] eq {OK}}
		}
		
        assert_equal 60 [r tacalc 1 1019 mytahit2]
    }
	
	test {TAHIT - constant rate for 1.5 length} {
	
	}
	
	test {TAHIT - constant rate for 2 length} {
	
	}
	
	test {TAHIT - constant rate for 2 length with gap (aligned)} {
	
	}
	
	test {TAHIT - constant rate for 2 length with gap (not aligned)} {
	
	}
}
