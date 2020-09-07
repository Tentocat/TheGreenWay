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
	.eabi_attribute 25, 1 @ Tag_ABI_align_preserved = 8-by