# Inside the REM Computer

## Edition 2 --- Workbook Revision

# Part II --- Assembly Software Engineering

# Chapter 3 --- Writing Maintainable REM Assembly

Assembly programming on REM is not just writing instructions.

The instruction set is the smallest part of the problem.

A maintainable assembly program requires:

-   clear module boundaries;
-   documented interfaces;
-   defined register ownership;
-   controlled memory ownership;
-   predictable error handling.

The objective is not clever code.

The objective is code that another systems programmer can understand and
debug.

------------------------------------------------------------------------

# 3.1 The REM ABI is the contract

Every function call depends on agreement between the caller and callee.

The REM ABI defines that agreement.

  Register   Purpose
  ---------- --------------------
  r0         constant zero
  r1-r4      function arguments
  r1         return value
  r13        stack pointer
  r14        link register

The complete ABI document remains the authority.

------------------------------------------------------------------------

# 3.2 Function contracts

Every assembly function should document:

## Purpose

What does this function provide?

Example:

    buffer_insert()

    Insert one character into the document buffer.

## Inputs

Where does information arrive?

Example:

    r1 = character

    r2 = position

## Outputs

Where does the result return?

Example:

    r1 = success or error code

## Registers modified

A caller must know what survives a function call.

Example:

    Clobbers:

    r5

    r6

## Memory ownership

Document who owns allocated memory.

Example:

    Caller provides buffer.

    Function does not free buffer.

------------------------------------------------------------------------

# 3.3 Stack discipline

The REM stack grows downward.

The stack is used for:

-   saved state;
-   local variables;
-   temporary values.

The stack must maintain:

    16-byte alignment

A stack error often appears later as:

-   corrupted return address;
-   incorrect function result;
-   invalid memory access.

------------------------------------------------------------------------

# 3.4 Assembly modules

A useful project separates responsibilities.

Example:

    remedit/

        src/

            terminal.S

            keyboard.S

            buffer.S

            editor.S

        tests/

            buffer_test.S

        Makefile

Each module should expose a small interface.

Example:

    buffer_insert()

    buffer_delete()

    buffer_split()

The editor should not know how the buffer stores memory internally.

------------------------------------------------------------------------

# 3.5 Assembly debugging discipline

When a program fails, reduce the problem.

Do not debug:

    Editor

    +

    terminal

    +

    filesystem

    +

    kernel

when the actual failure is:

    buffer_insert()

    returns the wrong address

Create small tests.

A good test proves one assumption.

------------------------------------------------------------------------

# Chapter 4 --- libc, Syscalls and the Kernel

REM provides a hosted programming environment, but the programmer must
understand the layers.

    Application

          |

          v

    Static musl libc / runtime subset

          |

          v

    Syscall interface

          |

          v

    Linux kernel

          |

          v

    REM CPU

------------------------------------------------------------------------

# 4.1 Static musl support

REM includes a MyEmulator2 musl port.

Current supported path:

    C source

        |

        v

    REM GCC compiler driver

        |

        v

    static musl libc

        |

        v

    ELF executable

The supported model is static linking.

Important limitations:

-   dynamic linking is incomplete;
-   shared libraries are not the normal deployment model;
-   complete POSIX coverage is not assumed.

Use the verified toolchain.

------------------------------------------------------------------------

# 4.2 When to use libc

Use the available static musl environment for normal application work
where supported.

Examples:

-   strings;
-   formatted output;
-   file handling;
-   memory management.

If functionality is unavailable or being tested at a lower level, use
the syscall interface directly.

------------------------------------------------------------------------

# 4.3 REM syscall convention

The syscall boundary uses the REM Linux convention.

Registers:

    r1

        syscall number

        return value


    r2-r7

        syscall arguments

A syscall wrapper should hide this detail from the rest of the
application.

------------------------------------------------------------------------

# 4.4 Example syscall wrapper

Do not place raw syscall setup throughout the application.

Create a wrapper module:

    syscall.S

Example interface:

    sys_write()

    Input:

    r1 = file descriptor

    r2 = buffer address

    r3 = length


    Return:

    r1 = result

The application uses:

    call sys_write

rather than knowing kernel register details.

------------------------------------------------------------------------

# 4.5 Why wrappers matter

Without wrappers:

    editor

        |

        contains syscall details

        |

        contains error handling

With wrappers:

    editor

        |

        v

    syscall module

        |

        v

    kernel

The application remains portable within the REM environment.

------------------------------------------------------------------------

# 4.6 Process interfaces

Do not assume desktop Linux behaviour.

The available process model is defined by the REM Linux implementation.

Use the documented interfaces:

-   supported syscalls;
-   supported process creation model;
-   supported executable loading.

------------------------------------------------------------------------

# Chapter results

## Chapter 3 result

Project:

    add-test

Proves:

-   assembler works;
-   linker works;
-   ABI calls work;
-   return values work.

------------------------------------------------------------------------

## Chapter 4 result

Project:

    syscall-test

Proves:

-   userspace enters the kernel;
-   syscall arguments are correct;
-   return values are handled.

Status:

    Host QEMU:

    verify with current regression environment


    Fold:

    requires device validation
