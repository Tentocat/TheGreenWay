@ vim: set tabstop=8 softtabstop=8 shiftwidth=8 noexpandtab syntax=armasm:
/**********************************************************************
 * Copyright (c) 2014 Wladimir J. van der Laan                        *
 * Distributed under the MIT software license, see the accompanying   *
 * file COPYING or http://www.opensource.org/licenses/mit-license.php.*
 **********************************************************************/
/*
ARM implementation of field_10x26 inner loops.

Note:

- To avoid unnecessary loads and make use of available registers, two
  'passes' have every time been interleaved, with the odd passes accumulating c' and d' 
  which will be added to c and d respectively in the even passes

*/

	.syntax unified
	.arch armv7-a
	@ eabi attributes - see readelf -A
	.eabi_attribute 8, 1  @ Tag_ARM_ISA_use = yes
	.eabi_attribute 9, 0  @ Tag_Thumb_ISA_use = no
	.eabi_attribute 10, 0 @ Tag_FP_arch = none
	.eabi_attribute 24, 1 @ Tag_ABI_align_needed = 8-byte
	.eabi_attribute 25, 1 @ Tag_ABI_align_preserved = 8-byte, except leaf SP
	.eabi_attribute 30, 2 @ Tag_ABI_optimization_goals = Aggressive Speed
	.eabi_attribute 34, 1 @ Tag_CPU_unaligned_access = v6
	.text

	@ Field constants
	.set field_R0, 0x3d10
	.set field_R1, 0x400
	.set field_not_M, 0xfc000000	@ ~M = ~0x3ffffff

	.align	2
	.global secp256k1_fe_mul_inner
	.type	secp256k1_fe_mul_inner, %function
	@ Arguments:
	@  r0  r      Restrict: can overlap with a, not with b
	@  r1  a
	@  r2  b
	@ Stack (total 4+10*4 = 44)
	@  sp + #0        saved 'r' pointer
	@  sp + #4 + 4*X  t0,t1,t2,t3,t4,t5,t6,t7,u8,t9
secp256k1_fe_mul_inner:
	stmfd	sp!, {r4, r5, r6, r7, r8, r9, r10, r11, r14}
	sub	sp, sp, #48			@ frame=44 + alignment
	str     r0, [sp, #0]			@ save result address, we need it only at the end

	/******************************************
	 * Main computation code.
	 ******************************************

	Allocation:
	    r0,r14,r7,r8   scratch
	    r1       a (pointer)
	    r2       b (pointer)
	    r3:r4    c
	    r5:r6    d
	    r11:r12  c'
	    r9:r10   d'

	Note: do not write to r[] here, it may overlap with a[]
	*/

	/* A - interleaved with B */
	ldr	r7, [r1, #0*4]			@ a[0]
	ldr	r8, [r2, #9*4]			@ b[9]
	ldr	r0, [r1, #1*4]			@ a[1]
	umull	r5, r6, r7, r8			@ d = a[0] * b[9]
	ldr	r14, [r2, #8*4]			@ b[8]
	umull	r9, r10, r0, r8			@ d' = a[1] * b[9]
	ldr	r7, [r1, #2*4]			@ a[2]
	umlal	r5, r6, r0, r14			@ d += a[1] * b[8]
	ldr	r8, [r2, #7*4] 			@ b[7]
	umlal	r9, r10, r7, r14		@ d' += a[2] * b[8]
	ldr	r0, [r1, #3*4]   		@ a[3]
	umlal	r5, r6, r7, r8   		@ d += a[2] * b[7]
	ldr	r14, [r2, #6*4]   		@ b[6]
	umlal	r9, r10, r0, r8  		@ d' += a[3] * b[7]
	ldr	r7, [r1, #4*4]   		@ a[4]
	umlal	r5, r6, r0, r14   		@ d += a[3] * b[6]
	ldr	r8, [r2, #5*4]   		@ b[5]
	umlal	r9, r10, r7, r14  		@ d' += a[4] * b[6]
	ldr	r0, [r1, #5*4]   		@ a[5]
	umlal	r5, r6, r7, r8   		@ d += a[4] * b[5]
	ldr	r14, [r2, #4*4]   		@ b[4]
	umlal	r9, r10, r0, r8  		@ d' += a[5] * b[5]
	ldr	r7, [r1, #6*4]   		@ a[6]
	umlal	r5, r6, r0, r14   		@ d += a[5] * b[4]
	ldr	r8, [r2, #3*4]   		@ b[3]
	umlal	r9, r10, r7, r14  		@ d' += a[6] * b[4]
	ldr	r0, [r1, #7*4]   		@ a[7]
	umlal	r5, r6, r7, r8   		@ d += a[6] * b[3]
	ldr	r14, [r2, #2*4]   		@ b[2]
	umlal	r9, r10, r0, r8  		@ d' += a[7] * b[3]
	ldr	r7, [r1, #8*4]   		@ a[8]
	umlal	r5, r6, r0, r14   		@ d += a[7] * b[2]
	ldr	r8, [r2, #1*4]   		@ b[1]
	umlal	r9, r10, r7, r14  		@ d' += a[8] * b[2]
	ldr	r0, [r1, #9*4]   		@ a[9]
	umlal	r5, r6, r7, r8   		@ d += a[8] * b[1]
	ldr	r14, [r2, #0*4]   		@ b[0]
	umlal	r9, r10, r0, r8  		@ d' += a[9] * b[1]
	ldr	r7, [r1, #0*4]   		@ a[0]
	umlal	r5, r6, r0, r14   		@ d += a[9] * b[0]
	@ r7,r14 used in B

	bic	r0, r5, field_not_M 		@ t9 = d & M
	str     r0, [sp, #4 + 4*9]
	mov	r5, r5, lsr #26     		@ d >>= 26 
	orr	r5, r5, r6, asl #6
	mov     r6, r6, lsr #26

	/* B */
	umull	r3, r4, r7, r14   		@ c = a[0] * b[0]
	adds	r5, r5, r9       		@ d += d'
	adc	r6, r6, r10

	bic	r0, r5, field_not_M 		@ u0 = d & M
	mov	r5, r5, lsr #26     		@ d >>= 26
	orr	r5, r5, r6, asl #6
	mov     r6, r6, lsr #26
	movw    r14, field_R0			@ c += u0 * R0
	umlal   r3, r4, r0, r14

	bic	r14, r3, field_not_M 		@ t0 = c & M
	str	r14, [sp, #4 + 0*4]
	mov	r3, r3, lsr #26     		@ c >>= 26
	orr	r3, r3, r4, asl #6
	mov     r4, r4, lsr #26
	mov     r14, field_R1			@ c += u0 * R1
	umlal   r3, r4, r0, r14

	/* C - interleaved with D */
	ldr	r7, [r1, #0*4]   		@ a[0]
	ldr	r8, [r2, #2*4]   		@ b[2]
	ldr	r14, [r2, #1*4]   		@ b[1]
	umull	r11, r12, r7, r8   		@ c' = a[0] * b[2]
	ldr	r0, [r1, #1*4]   		@ a[1]
	umlal   r3, r4, r7, r14   		@ c += a[0] * b[1]
	ldr	r8, [r2, #0*4]   		@ b[0]
	umlal   r11, r12, r0, r14   		@ c' += a[1] * b[1]
	ldr	r7, [r1, #2*4]   		@ a[2]
	umlal   r3, r4, r0, r8   		@ c += a[1] * b[0]
	ldr	r14, [r2, #9*4]   		@ b[9]
	umlal   r11, r12, r7, r8   		@ c' += a[2] * b[0]
	ldr	r0, [r1, #3*4]   		@ a[3]
	umlal	r5, r6, r7, r14   		@ d += a[2] * b[9]
	ldr	r8, [r2, #8*4]   		@ b[8]
	umull	r9, r10, r0, r14   		@ d' = a[3] * b[9]
	ldr	r7, [r1, #4*4]   		@ a[4]
	umlal	r5, r6, r0, r8   		@ d += a[3] * b[8]
	ldr	r14, [r2, #7*4]   		@ b[7]
	umlal	r9, r10, r7, r8   		@ d' += a[4] * b[8]
	ldr	r0, [r1, #5*4]   		@ a[5]
	umlal	r5, r6, r7, r14   		@ d += a[4] * b[7]
	ldr	r8, [r2, #6*4]   		@ b[6]
	umlal	r9, r10, r0, r14   		@ d' += a[5] * b[7]
	ldr	r7, [r1, #6*4]   		@ a[6]
	umlal	r5, r6, r0, r8   		@ d += a[5] * b[6]
	ldr	r14, [r2, #5*4]   		@ b[5]
	umlal	r9, r10, r7, r8   		@ d' += a[6] * b[6]
	ldr	r0, [r1, #7*4]   		@ a[7]
	umlal	r5, r6, r7, r14   		@ d += a[6] * b[5]
	ldr	r8, [r2, #4*4]   		@ b[4]
	umlal	r9, r10, r0, r14   		@ d' += a[7] * b[5]
	ldr	r7, [r1, #8*4]   		@ a[8]
	umlal	r5, r6, r0, r8   		@ d += a[7] * b[4]
	ldr	r14, [r2, #3*4]   		@ b[3]
	umlal	r9, r10, r7, r8   		@ d' += a[8] * b[4]
	ldr	r0, [r1, #9*4]   		@ a[9]
	umlal	r5, r6, r7, r14   		@ d += a[8] * b[3]
	ldr	r8, [r2, #2*4]   		@ b[2]
	umlal	r9, r10, r0, r14   		@ d' += a[9] * b[3]
	umlal	r5, r6, r0, r8   		@ d += a[9] * b[2]

	bic	r0, r5, field_not_M 		@ u1 = d & M
	mov	r5, r5, lsr #26     		@ d >>= 26
	orr	r5, r5, r6, asl #6
	mov     r6, r6, lsr #26
	movw    r14, field_R0			@ c += u1 * R0
	umlal   r3, r4, r0, r14

	bic	r14, r3, field_not_M 		@ t1 = c & M
	str	r14, [sp, #4 + 1*4]
	mov	r3, r3, lsr #26     		@ c >>= 26
	orr	r3, r3, r4, asl #6
	mov     r4, r4, lsr #26
	mov     r14, field_R1			@ c += u1 * R1
	umlal   r3, r4, r0, r14

	/* D */
	adds	r3, r3, r11			@ c += c'
	adc	r4, r4, r12
	adds	r5, r5, r9			@ d += d'
	adc	r6, r6, r10

	bic	r0, r5, field_not_M 		@ u2 = d & M
	mov	r5, r5, lsr #26     		@ d >>= 26
	orr	r5, r5, r6, asl #6
	mov     r6, r6, lsr #26
	movw    r14, field_R0			@ c += u2 * R0
	umlal   r3, r4, r0, r14

	bic	r14, r3, field_not_M 		@ t2 = c & M
	str	r14, [sp, #4 + 2*4]
	mov	r3, r3, lsr #26     		@ c >>= 26
	orr	r3, r3, r4, asl #6
	mov     r4, r4, lsr #26
	mov     r14, field_R1			@ c += u2 * R1
	umlal   r3, r4, r0, r14

	/* E - interleaved with F */
	ldr	r7, [r1, #0*4]   		@ a[0]
	ldr	r8, [r2, #4*4]   		@ b[4]
	umull	r11, r12, r7, r8   		@ c' = a[0] * b[4]
	ldr	r8, [r2, #3*4]   		@ b[3]
	umlal   r3, r4, r7, r8   		@ c += a[0] * b[3]
	ldr	r7, [r1, #1*4]   		@ a[1]
	umlal   r11, r12, r7, r8   		@ c' += a[1] * b[3]
	ldr	r8, [r2, #2*4]   		@ b[2]
	umlal   r3, r4, r7, r8   		@ c += a[1] * b[2]
	ldr	r7, [r1, #2*4]   		@ a[2]
	umlal   r11, r12, r7, r8   		@ c' += a[2] * b[2]
	ldr	r8, [r2, #1*4]   		@ b[1]
	umlal   r3, r4, r7, r8   		@ c += a[2] * b[1]
	ldr	r7, [r1, #3*4]   		@ a[3]
	umlal   r11, r12, r7, r8   		@ c' += a[3] * b[1]
	ldr	r8, [r2, #0*4]   		@ b[0]
	umlal   r3, r4, r7, r8   		@ c += a[3] * b[0]
	ldr	r7, [r1, #4*4]   		@ a[4]
	umlal   r11, r12, r7, r8   		@ c' += a[4] * b[0]
	ldr	r8, [r2, #9*4]   		@ b[9]
	umlal	r5, r6, r7, r8   		@ d += a[4] * b[9]
	ldr	r7, [r1, #5*4]   		@ a[5]
	umull	r9, r10, r7, r8   		@ d' = a[5] * b[9]
	ldr	r8, [r2, #8*4]   		@ b[8]
	umlal	r5, r6, r7, r8   		@ d += a[5] * b[8]
	ldr	r7, [r1, #6*4]   		@ a[6]
	umlal	r9, r10, r7, r8   		@ d' += a[6] * b[8]
	ldr	r8, [r2, #7*4]   		@ b[7]
	umlal	r5, r6, r7, r8   		@ d += a[6] * b[7]
	ldr	r7, [r1, #7*4]   		@ a[7]
	umlal	r9, r10, r7, r8   		@ d' += a[7] * b[7]
	ldr	r8, [r2, #6*4]   		@ b[6]
	umlal	r5, r6, r7, r8   		@ d += a[7] * b[6]
	ldr	r7, [r1, #8*4]   		@ a[8]
	umlal	r9, r10, r7, r8   		@ d' += a[8] * b[6]
	ldr	r8, [r2, #5*4]   		@ b[5]
	umlal	r5, r6, r7, r8   		@ d += a[8] * b[5]
	ldr	r7, [r1, #9*4]   		@ a[9]
	umlal	r9, r10, r7, r8   		@ d' += a[9] * b[5]
	ldr	r8, [r2, #4*4]   		@ b[4]
	umlal	r5, r6, r7, r8   		@ d += a[9] * b[4]

	bic	r0, r5, field_not_M 		@ u3 = d & M
	mov	r5, r5, lsr #26     		@ d >>= 26
	orr	r5, r5, r6, asl #6
	mov     r6, r6, lsr #26
	movw    r14, field_R0			@ c += u3 * R0
	umlal   r3, r4, r0, r14

	bic	r14, r3, field_not_M 		@ t3 = c & M
	str	r14, [sp, #4 + 3*4]
	mov	r3, r3, lsr #26     		@ c >>= 26
	orr	r3, r3, r4, asl #6
	mov     r4, r4, lsr #26
	mov     r14, field_R1			@ c += u3 * R1
	umlal   r3, r4, r0, r14

	/* F */
	adds	r3, r3, r11			@ c += c'
	adc	r4, r4, r12
	adds	r5, r5, r9			@ d += d'
	adc	r6, r6, r10

	bic	r0, r5, field_not_M 		@ u4 = d & M
	mov	r5, r5, lsr #26     		@ d >>= 26
	orr	r5, r5, r6, asl #6
	mov     r6, r6, lsr #26
	movw    r14, field_R0			@ c += u4 * R0
	umlal   r3, r4, r0, r14

	bic	r14, r3, field_not_M 		@ t4 = c & M
	str	r14, [sp, #4 + 4*4]
	mov	r3, r3, lsr #26     		@ c >>= 26
	orr	r3, r3, r4, asl #6
	mov     r4, r4, lsr #26
	mov     r14, field_R1			@ c += u4 * R1
	umlal   r3, r4, r0, r14

	/* G - interleaved with H */
	ldr	r7, [r1, #0*4]   		@ a[0]
	ldr	r8, [r2, #6*4]   		@ b[6]
	ldr	r14, [r2, #5*4]   		@ b[5]
	umull	r11, r12, r7, r8   		@ c' = a[0] * b[6]
	ldr	r0, [r1, #1*4]   		@ a[1]
	umlal   r3, r4, r7, r14   		@ c += a[0] * b[5]
	ldr	r8, [r2, #4*4]   		@ b[4]
	umlal   r11, r12, r0, r14   		@ c' += a[1] * b[5]
	ldr	r7, [r1, #2*4]   		@ a[2]
	umlal   r3, r4, r0, r8   		@ c += a[1] * b[4]
	ldr	r14, [r2, #3*4]   		@ b[3]
	umlal   r11, r12, r7, r8   		@ c' += a[2] * b[4]
	ldr	r0, [r1, #3*4]   		@ a[3]
	umlal   r3, r4, r7, r14   		@ c += a[2] * b[3]
	ldr	r8, [r2, #2*4]   		@ b[2]
	umlal   r11, r12, r0, r14   		@ c' += a[3] * b[3]
	ldr	r7, [r1, #4*4]   		@ a[4]
	umlal   r3, r4, r0, r8   		@ c += a[3] * b[2]
	ldr	r14, [r2, #1*4]   		@ b[1]
	umlal   r11, r12, r7, r8   		@ c' += a[4] * b[2]
	ldr	r0, [r1, #5*4]   		@ a[5]
	umlal   r3, r4, r7, r14   		@ c += a[4] * b[1]
	ldr	r8, [r2, #0*4]   		@ b[0]
	umlal   r11, r12, r0, r14   		@ c' += a[5] * b[1]
	ldr	r7, [r1, #6*4]   		@ a[6]
	umlal   r3, r4, r0, r8   		@ c += a[5] * b[0]
	ldr	r14, [r2, #9*4]   		@ b[9]
	umlal   r11, r12, r7, r8   		@ c' += a[6] * b[0]
	ldr	r0, [r1, #7*4]   		@ a[7]
	umlal	r5, r6, r7, r14   		@ d += a[6] * b[9]
	ldr	r8, [r2, #8*4]   		@ b[8]
	umull	r9, r10, r0, r14   		@ d' = a[7] * b[9]
	ldr	r7, [r1, #8*4]   		@ a[8]
	umlal	r5, r6, r0, r8   		@ d += a[7] * b[8]
	ldr	r14, [r2, #7*4]   		@ b[7]
	umlal	r9, r10, r7, r8   		@ d' += a[8] * b[8]
	ldr	r0, [r1, #9*4]   		@ a[9]
	umlal	r5, r6, r7, r14   		@ d += a[8] * b[7]
	ldr	r8, [r2, #6*4]   		@ b[6]
	umlal	r9, r10, r0, r14   		@ d' += a[9] * b[7]
	umlal	r5, r6, r0, r8   		@ d += a[9] * b[6]

	bic	r0, r5, field_not_M 		@ u5 = d & M
	mov	r5, r5, lsr #26     		@ d >>= 26
	orr	r5, r5, r6, asl #6
	mov     r6, r6, lsr #26
	movw    r14, field_R0			@ c += u5 * R0
	umlal   r3, r4, r0, r14

	bic	r14, r3, field_not_M 		@ t5 = c & M
	str	r14, [sp, #4 + 5*4]
	mov	r3, r3, lsr #26     		@ c >>= 26
	orr	r3, r3, r4, asl #6
	mov     r4, r4, lsr #26
	mov     r14, field_R1			@ c += u5 * R1
	umlal   r3, r4, r0, r14

	/* H */
	adds	r3, r3, r11			@ c += c'
	adc	r4, r4, r12
	adds	r5, r5, r9			@ d += d'
	adc	r6, r6, r10

	bic	r0, r5, field_not_M 		@ u6 = d & M
	mov	r5, r5, lsr #26     		@ d >>= 26
	orr	r5, r5, r6, asl #6
	mov     r6, r6, lsr #26
	movw    r14, field_R0			@ c += u6 * R0
	umlal   r3, r4, r0, r14

	bic	r14, r3, field_not_M 		@ t6 = c & M
	str	r14, [sp, #4 + 6*4]
	mov	r3, r3, lsr #26     		@ c >>= 26
	orr	r3, r3, r4, asl #6
	mov     r4, r4, lsr #26
	mov     r14, field_R1			@ c += u6 * R1
	umlal   r3, r4, r0, r14

	/* I - interleaved with J */
	ldr	r8, [r2, #8*4]   		@ b[8]
	ldr	r7, [r1, #0*4]   		@ a[0]
	ldr	r14, [r2, #7*4]   		@ b[7]
	umull   r11, r12, r7, r8   		@ c' = a[0] * b[8]
	ldr	r0, [r1, #1*4]   		@ a[1]
	umlal   r3, r4, r7, r14   		@ c += a[0] * b[7]
	ldr	r8, [r2, #6*4]   		@ b[6]
	umlal   r11, r12, r0, r14   		@ c' += a[1] * b[7]
	ldr	r7, [r1, #2*4]   		@ a[2]
	umlal   r3, r4, r0, r8   		@ c += a[1] * b[6]
	ldr	r14, [r2, #5*4]   		@ b[5]
	umlal   r11, r12, r7, r8   		@ c' += a[2] * b[6]
	ldr	r0, [r1, #3*4