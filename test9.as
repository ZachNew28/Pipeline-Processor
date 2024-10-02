	lw	0	4	mask
	add	0	0	5
	lw	0	2	i
	lw	0	3	max
	lw	0	1	one
loop	beq	2	3	end
	nor	2	2	6
	nor	4	6	6
	beq	6	0	skip
	add	5	2	5
skip	add	2	1	2
	beq	0	0	loop
end	halt
mask	.fill	-4
i	.fill	3
max	.fill	7
one	.fill	1
