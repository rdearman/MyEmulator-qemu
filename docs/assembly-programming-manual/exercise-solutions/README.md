# Exercise solutions

This directory is the separate source location for complete reference
solutions to the 20 exercises in chapter 14 of the assembly manual. Solutions
must remain separate from the exercise descriptions so the challenges can be
attempted offline without exposing an answer.

Each solution added here must include:

1. A complete `.S` source file, not an excerpt.
2. The expected command-line arguments and output.
3. A build and run record using the actual REM toolchain (with its historical
   `myemulator2-elf` command names).
4. A statement identifying whether execution was verified under host QEMU,
   inside the REM guest, or on a Fold.

The `Makefile` assembles and links any `exercise-*.S` files present in this
directory. It intentionally does not report runtime success: assembly/link
success and guest execution are separate acceptance criteria. At present the
repository contains no verified reference solution files in this directory;
the exercise chapter is therefore usable independently while solutions are
added and tested one at a time.
