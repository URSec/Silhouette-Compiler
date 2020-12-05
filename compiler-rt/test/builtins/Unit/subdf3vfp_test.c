// RUN: %clang_builtins %s %librt -o %t && %run %t
//===-- subdf3vfp_test.c - Test __subdf3vfp -------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file tests __subdf3vfp for the compiler_rt library.
//
//===----------------------------------------------------------------------===//

#include "int_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


#if defined(__arm__) && defined(__ARM_FP) && (__ARM_FP & 0x8)
extern COMPILER_RT_ABI double __subdf3vfp(double a, double b);

int test__subdf3vfp(double a, double b)
{
    double actual = __subdf3vfp(a, b);
    double expected = a - b;
    if (actual != expected)
        printf("error in test__subdf3vfp(%f, %f) = %f, expected %f\n",
               a, b, actual, expected);
    return actual != expected;
}
#endif

int main()
{
#if defined(__arm__) && defined(__ARM_FP) && (__ARM_FP & 0x8)
    if (test__subdf3vfp(1.0, 1.0))
        return 1;
    if (test__subdf3vfp(1234.567, 765.4321))
        return 1;
    if (test__subdf3vfp(-123.0, -678.0))
        return 1;
    if (test__subdf3vfp(0.0, -0.0))
        return 1;
#else
    printf("skipped\n");
#endif
    return 0;
}
