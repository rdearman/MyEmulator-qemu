	.text
	.global archive_result
	.type archive_result,@function
	.extern c_value
archive_result:
	call c_value
	addi r1, r1, 5
	ret
