//===-- aeabi_fcmp.S - EABI fcmp* implementation ---------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../assembly.h"

// int __aeabi_fcmp{eq,lt,le,ge,gt}(float a, float b) {
//   int result = __{eq,lt,le,ge,gt}sf2(a, b);
//   if (result {==,<,<=,>=,>} 0) {
//     return 1;
//   } else {
//     return 0;
//   }
// }

#if defined(COMPILER_RT_ARMHF_TARGET)
#  define CONVERT_FCMP_ARGS_TO_SF2_ARGS                    \
        vmov      s0, r0                         SEPARATOR \
        vmov      s1, r1
#else
#  define CONVERT_FCMP_ARGS_TO_SF2_ARGS
#endif

#ifdef SILHOUETTE
#define PROLOGUE                                           \
        SAVE_TO_SS(lr)                           SEPARATOR \
        sub       sp, #8                         SEPARATOR \
        strt      r4, [sp]                       SEPARATOR \
        strt      lr, [sp, #4]
#define EPILOGUE                                           \
        pop       { r4 }                         SEPARATOR \
        add       sp, #4                         SEPARATOR \
        LOAD_FROM_SS(pc)
#else
#define PROLOGUE push {r4, lr}
#define EPILOGUE pop {r4, pc}
#endif

#define DEFINE_AEABI_FCMP(cond)                            \
        .syntax unified                          SEPARATOR \
        .p2align 2                               SEPARATOR \
DEFINE_COMPILERRT_FUNCTION(__aeabi_fcmp ## cond)           \
        PROLOGUE                                 SEPARATOR \
        CONVERT_FCMP_ARGS_TO_SF2_ARGS            SEPARATOR \
        bl        SYMBOL_NAME(__ ## cond ## sf2) SEPARATOR \
        cmp       r0, #0                         SEPARATOR \
        b ## cond 1f                             SEPARATOR \
        movs      r0, #0                         SEPARATOR \
        EPILOGUE                                 SEPARATOR \
1:                                               SEPARATOR \
        movs      r0, #1                         SEPARATOR \
        EPILOGUE                                 SEPARATOR \
END_COMPILERRT_FUNCTION(__aeabi_fcmp ## cond)

DEFINE_AEABI_FCMP(eq)
DEFINE_AEABI_FCMP(lt)
DEFINE_AEABI_FCMP(le)
DEFINE_AEABI_FCMP(ge)
DEFINE_AEABI_FCMP(gt)

NO_EXEC_STACK_DIRECTIVE

