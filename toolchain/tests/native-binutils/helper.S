	.text
	.global answer
	.type answer,@function
answer:
	li r1, 42
	ret
	.size answer, .-answer
