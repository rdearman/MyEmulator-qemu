# Inside the REM Computer

## Edition 2 — Workbook Revision

# Appendices — Revision 2

---

# Appendix A — REM Programming Checklist

Use this checklist before starting a new REM program.

## Project setup

```
[ ] src/ created

[ ] tests/ created

[ ] build/ separated from source

[ ] docs/ updated
```

---

## Architecture

Before writing code:

```
[ ] define module boundaries

[ ] define function contracts

[ ] define ownership rules

[ ] define error handling
```

Every function documents:

```
inputs

outputs

modified registers

memory ownership

failure cases
```

---

# ABI checklist

## Normal function ABI

For normal function calls:

```
r0

constant zero
```

Arguments:

```
r1-r4
```

Return value:

```
r1
```

Special registers:

```
r13

stack pointer


r14

link register
```

Maintain stack alignment according to the REM ABI.

---

## Linux syscall ABI

Do not confuse function calls with syscalls.

Linux syscall convention:

```
r1

syscall number

return value
```

Arguments:

```
r2-r7
```

A normal function argument in r1 is not the same thing as a syscall argument in r2.

---

# Debugging checklist

When a program fails:

```
[ ] assembler succeeded?

[ ] linker succeeded?

[ ] ELF generated?

[ ] loader succeeded?

[ ] syscall number correct?

[ ] syscall arguments in correct registers?

[ ] return value checked?

[ ] ABI followed?

[ ] stack alignment correct?

[ ] memory corruption possible?
```

---

# Appendix B — Assembly Module Template

A REM assembly module should define a clear interface.

Example:

```asm
#
# module_name.S
#

.section .text

.global function_name

function_name:

    #
    # Contract:
    #
    # Input:
    #   r1 = argument
    #
    # Output:
    #   r1 = result
    #
    # Modified:
    #   documented registers
    #
    # Ownership:
    #   documented memory ownership
    #

    ret
```

---

## Module rules

A module defines:

```
public functions

private helpers

required data

register assumptions

error behaviour
```

Avoid hidden dependencies.

---

# Appendix C — Test Program Template

Every low-level feature should have a small executable test.

A test defines:

```
Input:

Expected result:

Exit status:

Failure condition:
```

Example:

```
Input:

add two numbers


Expected:

correct result register value


Success:

exit status 0


Failure:

exit status non-zero
```

---

## Test environment

Every test documents where it runs:

```
Host QEMU verified

Guest Linux target

Fold validation
```

---

## Test progression

Use:

```
small test

    |

    v

module test

    |

    v

application test
```

A ten-line test is often more useful than debugging a complete application.

---

# Appendix D — Makefile Template

REM projects should use the REM toolchain explicitly.

Example:

```make
PREFIX ?= myemulator2-elf

AS ?= $(PREFIX)-as
LD ?= $(PREFIX)-ld

TARGET=myprogram

SOURCES=main.S

OBJECTS=$(SOURCES:.S=.o)

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(LD) $(OBJECTS) -o $(TARGET)

%.o: %.S
	$(AS) $< -o $@

test:
	./run_tests

clean:
	rm -f $(OBJECTS) $(TARGET)
```

---

## C application builds

The supported hosted C path uses:

```
GCC

+

static musl libc

+

static linking
```

Dynamic linking should not be assumed.

---

# Appendix E — Terminal Escape Reference

Terminal communication uses bytes.

Escape character:

```
ESC = 0x1b
```

The terminal receives output sequences.

The keyboard sends input sequences.

These are different directions.

---

## Output examples

Clear screen:

```
ESC [ 2 J
```

Cursor position:

```
ESC [ row ; column H
```

Cursor movement:

```
ESC [ A

up


ESC [ B

down


ESC [ C

right


ESC [ D

left
```

---

## Input examples

Arrow keys are not single characters.

Example:

```
ESC [ A
```

becomes:

```
KEY_UP
```

The keyboard decoder performs this conversion.

---

## Terminal modes

Input behaviour is controlled through termios.

Important concepts:

```
canonical mode

raw mode

echo

signals
```

Function-key sequences vary between terminals.

Do not assume all terminals generate identical sequences.

---

# Appendix F — Debugging Decision Tree

## Build failure

```
Program does not build?

        |

        +-- assembler error?

        |

        +-- linker error?

        |

        +-- wrong object format?
```

---

## Program does not run

```
        |

        +-- loader failure?

        |

        +-- missing symbol?

        |

        +-- startup code problem?
```

---

## Runtime failure

```
        |

        +-- ABI violation?

        |

        +-- stack corruption?

        |

        +-- wrong register assumptions?

        |

        +-- memory error?

        |

        +-- syscall misuse?
```

---

## Process/build integration failure

Check:

```
[ ] clone succeeded?

[ ] execve path correct?

[ ] argv/envp correct?

[ ] wait4 result decoded?

[ ] stdout captured?

[ ] stderr captured?
```

---

## Terminal failure

Check:

```
[ ] raw mode restored?

[ ] termios state restored?

[ ] terminal reset required?
```

---

# Final reminder

New CPU architectures are built through verified layers:

```
small programs

    |

    v

verified assumptions

    |

    v

larger systems
```

Reduce unknowns before adding complexity.
