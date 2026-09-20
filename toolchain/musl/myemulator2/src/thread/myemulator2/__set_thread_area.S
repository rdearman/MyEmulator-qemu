.global __set_thread_area
.hidden __set_thread_area
.type __set_thread_area,@function
__set_thread_area:
	mtsr tp, r1
	addi r1, r0, 0
	jr r14
