// RUN: %clang_builtins %s %librt -o %t && %run %t
//===-- truncdfsf2vfp_test.c - Test __truncdfsf2vfp -----------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//
//
// This file tests __truncdfsf2vfp for the compiler_rt library.
//
//===----------------------------------------------------------------------===//

#include "int_lib.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>


extern COMPILER_RT_ABI float __truncdfsf2vfp(double a);

#if defined(__arm__) && defined(__ARM_FP) && (__ARM_FP & 0x8)
int test__truncdfsf2vfp(double a)
{
    float actual = __truncdfsf2vfp(a);
    float expected = a;
    if (actual != expected)
        printf("error in test__truncdfsf2vfp(%f) = %f, expected %f\n",
               a, actual, expected);
    return actual != expected;
}
#endif

int main()
{
#if defined(__arm__) && defined(__ARM_FP) && (__ARM_FP & 0x8)
    if (test__truncdfsf2vfp(0.0))
        return 1;
    if (test__truncdfsf2vfp(1.0))
        return 1;
    if (test__truncdfsf2vfp(-1.0))
        return 1;
    if (test__truncdfsf2vfp(3.1415926535))
        return 1;
    if (test__truncdfsf2vfp(123.456))
        return 1;
#else
    printf("skipped\n");
#endif
    return 0;
}
