# Inside the REM Computer

## Edition 2 --- Workbook Revision

# Part VI --- Advanced REM Tools

# Chapter 17 --- REM System Monitor (Revised)

A system monitor is a tool for observing a running system.

However, the available information depends on the interfaces exposed by
REM Linux.

This chapter separates:

-   implemented tools;
-   host-QEMU tools;
-   future guest tools.

Do not assume that a normal desktop Linux `/proc` environment exists.

------------------------------------------------------------------------

# 17.1 Monitor architecture

The monitor should separate collection from display.

    Data source

          |

          v

    Monitor backend

          |

          v

    Display layer

          |

          v

    Terminal

The display should not depend on how information was collected.

------------------------------------------------------------------------

# 17.2 Process information

A process monitor may display:

    process identifier

    state

    command name

    resource information

The exact data source must be verified against the REM Linux
environment.

Status:

    Guest monitor:

    planned until kernel data interface is defined

------------------------------------------------------------------------

# 17.3 Memory information

Memory information may include:

    allocated memory

    address ranges

    stack information

    heap information

A user-space monitor cannot automatically inspect arbitrary processes.

The design must distinguish:

    current process information

    kernel-provided process information

    debugger-controlled inspection

------------------------------------------------------------------------

# 17.4 Register information

Register display requires special handling.

A normal system monitor cannot read another process's CPU registers.

Possible sources:

    current process debugging

    QEMU debugger interface

    future kernel debug interface

A register viewer therefore belongs primarily to the debugger, not the
system monitor.

------------------------------------------------------------------------

# 17.5 Performance information

Performance counters are only displayed if a verified interface exists.

Possible future data:

    instruction count

    execution time

    memory activity

Status:

    Future REM tool

------------------------------------------------------------------------

# Chapter 18 --- REM Debugger Front End (Revised)

A debugger is not the same as a system monitor.

It controls and inspects program execution.

The intended REM debugger architecture must be defined before
implementation.

Possible designs:

    Host QEMU debugger

    Guest debugger interface

    ptrace-style Linux interface

    REMEDIT front end over debug protocol

These are different projects.

------------------------------------------------------------------------

# 18.1 Debugger architecture

General model:

    Debugger UI

          |

          v

    Debug protocol

          |

          v

    Execution environment

The UI should not directly depend on CPU implementation details.

------------------------------------------------------------------------

# 18.2 Breakpoint workflow

Conceptual workflow:

    start program

          |

          v

    set breakpoint

          |

          v

    execute

          |

          v

    breakpoint reached

          |

          v

    inspect state

Availability depends on the selected debug interface.

------------------------------------------------------------------------

# 18.3 Register inspection

A debugger can display:

    r0-r15

    program counter

    status registers

The displayed register set must match the REM ABI.

------------------------------------------------------------------------

# 18.4 Memory inspection

A debugger memory command requires:

    address

    length

    format

Example:

    display 64 bytes at address X

------------------------------------------------------------------------

# 18.5 Single stepping

Single stepping:

    execute one instruction

    stop

    display state

Useful for:

-   startup debugging;
-   ABI mistakes;
-   syscall debugging.

Status:

    Host QEMU:

    available where supported


    Guest debugger:

    future

------------------------------------------------------------------------

# Chapter 19 --- REM Disassembly Tools (Revised)

This is the most immediately implementable advanced tool.

REM already has an ELF32 toolchain and target disassembler support.

------------------------------------------------------------------------

# 19.1 REM executable format

REM programs use:

    ELF32

    little endian

    32-bit instructions

The disassembler begins with:

    ELF file

        |

        v

    sections

        |

        v

    machine code

        |

        v

    instruction decoder

------------------------------------------------------------------------

# 19.2 Existing disassembly workflow

Example:

    myemulator2-elf-objdump -d program.elf

This should produce the assembly listing for a REM executable.

------------------------------------------------------------------------

# 19.3 Disassembler phases

A standalone REM disassembler performs:

    read ELF header

            |

            v

    locate code sections

            |

            v

    read symbols

            |

            v

    decode instructions

            |

            v

    print assembly

------------------------------------------------------------------------

# 19.4 Instruction decoding

The decoder performs:

    instruction word

            |

            v

    extract opcode

            |

            v

    extract registers/immediates

            |

            v

    format assembly

Unknown instructions must produce:

    unknown opcode

    address

    raw instruction value

rather than silently failing.

------------------------------------------------------------------------

# 19.5 Symbol lookup

A useful disassembler displays:

    main:

        instruction

        instruction

rather than only:

    00001000:

Symbol information comes from the ELF symbol table.

------------------------------------------------------------------------

# 19.6 Source mapping

Future integration:

    instruction address

            |

            v

    symbol lookup

            |

            v

    source location

This enables:

-   debugger stepping;
-   source navigation;
-   meaningful diagnostics.

------------------------------------------------------------------------

# Tests and status

## disassembler-test

Status:

    Implementable now

Input:

    known REM ELF executable

Expected:

    matching instruction listing

------------------------------------------------------------------------

## monitor-test

Status:

    Planned

Requires verified guest data interfaces.

------------------------------------------------------------------------

## debugger-test

Status:

    Host/QEMU validation only unless a guest debug ABI is created

Requires:

-   breakpoint mechanism;
-   register access;
-   execution control interface.
