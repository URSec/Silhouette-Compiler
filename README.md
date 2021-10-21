# The LLVM-Based Silhouette Compiler

This repository contains the source code of the LLVM-based Silhouette compiler.

## How to Compile Silhouette
Compiling the Silhouette compiler is exactly the same as compiling a
standard LLVM compiler. Therefore you can compile it in whatever way you
are used to compiling the LLVM compiler infrastructure.
If you are not familiar with compiling LLVM, please refer to [this
script](https://github.com/URSec/Silhouette-Evaluation/blob/master/build/build.llvm.sh)
to see how we built the compiler.

## How to Use Silhouette
To enable Silhouette transformations when compiling a C program, add the
following options to `CFLAGS`:
```shell
-mllvm -enable-arm-silhouette-str2strt    # Enable the store hardening pass
-mllvm -enable-arm-silhouette-shadowstack # Enable the shadow stack pass
-mllvm -enable-arm-silhouette-cfi         # Enable the CFI pass
```
Alternatively, you can use the following option to turn on all of the above
passes as a shortcut:
```shell
-mllvm -enable-arm-silhouette # Enable all of the Silhouette passes
```
Because Silhouette uses a parallel shadow stack, the compiler needs to
know the shadow stack offset relative to the regular stack.  This offset
is 14 MB by default and can be specified with a custom value using the
following option:
```shell
-mllvm -arm-silhouette-shadowstack-offset=XXX # Set the shadow stack offset to XXX
```
