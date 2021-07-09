//===-- save_restore_regs.S - Implement save/restore* ---------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===----------------------------------------------------------------------===//

#include "../assembly.h"

//
// When compiling C++ functions that need to handle thrown exceptions the
// compiler is required to save all registers and call __Unwind_SjLj_Register
// in the function prolog.  But when compiling for thumb1, there are
// no instructions to access the floating point registers, so the
// compiler needs to add a call to the helper function _save_vfp_d8_d15_regs
// written in ARM to save the float registers.  In the epilog, the compiler
// must also add a call to __restore_vfp_d8_d15_regs to restore those registers.
//

	.text
	.syntax unified

//
// Save registers d8-d15 onto stack
//
	.p2align 2
DEFINE_COMPILERRT_PRIVATE_FUNCTION(__save_vfp_d8_d15_regs)
#ifdef SILHOUETTE
	sub	sp, #64
	vmov	r0, r1, d8
	strt	r0, [sp, #0]
	strt	r1, [sp, #4]
	vmov	r0, r1, d9
	strt	r0, [sp, #8]
	strt	r1, [sp, #12]
	vmov	r0, r1, d10
	strt	r0, [sp, #16]
	strt	r1, [sp, #20]
	vmov	r0, r1, d11
	strt	r0, [sp, #24]
	strt	r1, [sp, #28]
	vmov	r0, r1, d12
	strt	r0, [sp, #32]
	strt	r1, [sp, #36]
	vmov	r0, r1, d13
	strt	r0, [sp, #40]
	strt	r1, [sp, #44]
	vmov	r0, r1, d14
	strt	r0, [sp, #48]
	strt	r1, [sp, #52]
	vmov	r0, r1, d15
	strt	r0, [sp, #56]
	strt	r1, [sp, #60]
#else
	vstmdb	sp!, {d8-d15}           // push registers d8-d15 onto stack
#endif
	bx      lr                      // return to prolog
END_COMPILERRT_FUNCTION(__save_vfp_d8_d15_regs)

NO_EXEC_STACK_DIRECTIVE

