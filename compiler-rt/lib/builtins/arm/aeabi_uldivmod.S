//===-- aeabi_uldivmod.S - EABI uldivmod implementation -------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../assembly.h"

// struct { uint64_t quot, uint64_t rem}
//        __aeabi_uldivmod(uint64_t numerator, uint64_t denominator) {
//   uint64_t rem, quot;
//   quot = __udivmoddi4(numerator, denominator, &rem);
//   return {quot, rem};
// }

#if defined(__MINGW32__)
#define __aeabi_uldivmod __rt_udiv64
#endif

        .syntax unified
        .p2align 2
DEFINE_COMPILERRT_FUNCTION(__aeabi_uldivmod)
#ifdef SILHOUETTE
        SAVE_TO_SS(lr)
        sub     sp, #8
        strt    r6, [sp]
        strt    lr, [sp, #4]
#else
        push	{r6, lr}
#endif
        sub	sp, sp, #16
        add	r6, sp, #8
#ifdef SILHOUETTE
        strt    r6, [sp]
#else
        str	r6, [sp]
#endif
#if defined(__MINGW32__)
        movs    r6, r0
        movs    r0, r2
        movs    r2, r6
        movs    r6, r1
        movs    r1, r3
        movs    r3, r6
#endif
        bl	SYMBOL_NAME(__udivmoddi4)
        ldr	r2, [sp, #8]
        ldr	r3, [sp, #12]
        add	sp, sp, #16
#ifdef SILHOUETTE
        pop     {r6}
        add     sp, #4
        LOAD_FROM_SS(pc)
#else
        pop	{r6, pc}
#endif
END_COMPILERRT_FUNCTION(__aeabi_uldivmod)

NO_EXEC_STACK_DIRECTIVE

