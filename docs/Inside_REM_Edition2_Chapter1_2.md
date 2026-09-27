# Inside the REM Computer

## Building Native Applications on a New Linux-Compatible CPU Architecture

# Part I --- The REM Environment

## Chapter 1 --- Welcome to REM Application Development

This book exists because a CPU manual is not a programming environment.

The REM architecture manuals describe the machine:

-   instruction set;
-   registers;
-   ABI;
-   assembler syntax;
-   executable format;
-   system interfaces.

They answer:

> "What instructions does the processor execute?"

They do not answer:

> "How does a programmer actually build software here?"

This book fills that gap.

It is a companion to the REM technical manuals. The manuals remain
authoritative. If this book disagrees with them, the manuals win.

The goal is practical:

Build native REM software.

------------------------------------------------------------------------

# 1.1 The machine is finished; now build the ecosystem

The transition from CPU development to application development requires
a change in thinking.

During CPU development the questions are:

-   Does the instruction decode correctly?
-   Does memory work?
-   Does Linux boot?
-   Does the kernel handle exceptions?

Application development asks different questions:

-   How do I organise a program?
-   How do I call functions?
-   How do I use files?
-   How do I debug failures?
-   How do I build larger software?

The software stack is:

    Applications

            |

            v

    Libraries and runtime support

            |

            v

    Linux kernel

            |

            v

    REM CPU architecture

The application programmer should normally work from the top down.

------------------------------------------------------------------------

# 1.2 Current REM development baseline

The target covered by this book is the REM 32-bit architecture.

The environment provides:

-   GNU assembler support;
-   GNU linker support;
-   ELF executable generation;
-   REM ABI conventions;
-   Linux-compatible execution environment.

The current environment must always be separated into:

## Implemented

Features demonstrated by testing.

## Planned

Features intended but not yet complete.

## Host tested

Validated under QEMU.

## Device tested

Validated on the target phone environment.

A working host emulator is not automatically a working phone deployment.

------------------------------------------------------------------------

# 1.3 What you already have

The intended programmer environment contains:

    assembler

    linker

    compiler support

    runtime support

    Linux userspace

    filesystem

The purpose of this book is not to rebuild Linux.

The purpose is to create software on top of it.

------------------------------------------------------------------------

# 1.4 What you do not immediately rebuild

A common trap when working on a new architecture is rebuilding
everything.

Do not begin by replacing existing facilities.

Before writing your own:

    malloc

    printf

    file handling

    process creation

    shell utilities

verify what already exists in the REM environment.

The first REM programs should prove the architecture, not duplicate
decades of operating system work.

------------------------------------------------------------------------

# 1.5 The first rule of systems programming

Build small things that prove assumptions before building large things
that hide failures.

A good first project:

    write text

    exit cleanly

A bad first project:

    complete editor

    without testing terminal output

Every layer should have a small test.

The progression:

    instruction

        |

        v

    function

        |

        v

    module

        |

        v

    application

------------------------------------------------------------------------

# Chapter 2 --- Your First REM Development Session

The first program should not be ambitious.

The purpose is to prove:

-   assembler works;
-   linker works;
-   executable format works;
-   program starts;
-   system interface works.

------------------------------------------------------------------------

# 2.1 Project structure

Use a structure that scales:

    hello-rem/

        src/

            main.S

        build/

        bin/

        Makefile

Keep generated files away from source.

------------------------------------------------------------------------

# 2.2 The first program

The first application should:

1.  enter at the correct Linux entry point;
2.  perform one useful operation;
3.  exit cleanly.

The exact startup sequence and syscall interface must follow the REM ABI
documentation.

The important engineering pattern is:

    initialisation

            |

            v

    application logic

            |

            v

    cleanup

            |

            v

    exit

------------------------------------------------------------------------

# 2.3 Understanding the build pipeline

The source does not become an executable in one step.

    source.S

        |

        v

    assembler

        |

        v

    object file

        |

        v

    linker

        |

        v

    ELF executable

Each stage can fail independently.

A disciplined programmer asks:

"Which stage failed?"

not:

"Why does the computer not work?"

------------------------------------------------------------------------

# 2.4 The role of Make

Make records relationships.

Example:

    program

    depends on:

    main.o

    terminal.o

If only one source file changes, only that part rebuilds.

A useful first Makefile should provide:

    all

    clean

    test

The build system is part of the program.

------------------------------------------------------------------------

# 2.5 First debugging checklist

When something fails:

## Assembler failure

Check:

-   syntax;
-   instruction names;
-   register names.

## Linker failure

Check:

-   missing symbols;
-   wrong object files;
-   incorrect linker script.

## Loader failure

Check:

-   executable format;
-   entry point;
-   permissions.

## Runtime failure

Check:

-   registers;
-   stack;
-   syscall arguments;
-   memory access.

Always find the first incorrect assumption.
