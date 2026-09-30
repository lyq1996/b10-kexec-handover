

	.align	4
	.global	kl_tramp_start
kl_tramp_start:
	mrs	x9, CurrentEL
	cmp	x9, #(2 << 2)
	b.eq	2f
1:	mrs	x10, sctlr_el1
	bic	x10, x10, #(1 << 0)
	bic	x10, x10, #(1 << 2)
	msr	sctlr_el1, x10
	isb
	b	3f
2:	mrs	x10, sctlr_el2
	bic	x10, x10, #(1 << 0)
	bic	x10, x10, #(1 << 2)
	msr	sctlr_el2, x10
	isb
3:	ic	iallu
	dsb	nsh
	isb
	mov	x0, x20
	mov	x1, xzr
	mov	x2, xzr
	mov	x3, xzr
	br	x21
	.global	kl_tramp_end
kl_tramp_end:
