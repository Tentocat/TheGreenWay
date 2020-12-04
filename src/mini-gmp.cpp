
/* mini-gmp, a minimalistic implementation of a GNU GMP subset.

   Contributed to the GNU project by Niels Möller

Copyright 1991-1997, 1999-2015 Free Software Foundation, Inc.

This file is part of the GNU MP Library.

The GNU MP Library is free software; you can redistribute it and/or modify
it under the terms of either:

  * the GNU Lesser General Public License as published by the Free
    Software Foundation; either version 3 of the License, or (at your
    option) any later version.

or

  * the GNU General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your option) any
    later version.

or both in parallel, as here.

The GNU MP Library is distributed in the hope that it will be useful, but
WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
for more details.

You should have received copies of the GNU General Public License and the
GNU Lesser General Public License along with the GNU MP Library.  If not,
see https://www.gnu.org/licenses/.  */

/* NOTE: All functions in this file which are not declared in
   mini-gmp.h are internal, and are not intended to be compatible
   neither with GMP nor with future versions of mini-gmp. */

/* Much of the material copied from GMP files, including: gmp-impl.h,
   longlong.h, mpn/generic/add_n.c, mpn/generic/addmul_1.c,
   mpn/generic/lshift.c, mpn/generic/mul_1.c,
   mpn/generic/mul_basecase.c, mpn/generic/rshift.c,
   mpn/generic/sbpi1_div_qr.c, mpn/generic/sub_n.c,
   mpn/generic/submul_1.c. */

/******************************************************************************
 * Copyright © 2014-2020 The Komodo Platform Developers.                      *
 *                                                                            *
 * See the AUTHORS, DEVELOPER-AGREEMENT and LICENSE files at                  *
 * the top-level directory of this distribution for the individual copyright  *
 * holder information and the developer policies on copyright and licensing.  *
 *                                                                            *
 * Unless otherwise agreed in a custom licensing agreement, no part of the    *
 * Komodo Platform software, including this file may be copied, modified,     *