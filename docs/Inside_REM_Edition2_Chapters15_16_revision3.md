# Inside the REM Computer

## Edition 2 — Workbook Revision

# Part V — Making REMEDIT a Development Environment

# Chapter 15 — Building from the Editor (Revision 3)

An editor becomes a development environment when it can control the development workflow.

The editor does not replace:

- assembler;
- linker;
- compiler;
- build system.

It coordinates them.

The workflow:

```
edit

 |

 v

save

 |

 v

build

 |

 v

capture result

 |

 v

fix source
```

---

# 15.1 Build launcher architecture

The editor should not contain process-management details.

Use a separate module:

```
process.S

    process_start()

    process_wait()

    process_result()
```

The editor calls the process layer.

The process layer handles REM Linux details.

---

# 15.2 Process module contracts

A function contract documents:

- inputs;
- outputs;
- modified state;
- ownership.

Example:

```
process_start()

Input:

    executable path

    argv array

    environment array

    working directory


Output:

    process handle


Ownership:

    caller owns argument storage
```

The exact register mapping follows the REM ABI.

---

# 15.3 REM syscall boundary

The Linux syscall convention is:

```
r1

    syscall number

    return value


r2-r7

    syscall arguments
```

The process module hides this from the editor.

The implementation uses the documented REM process lifecycle.

---

# 15.4 REM process lifecycle

The build launcher follows:

```
parent

    |

    v

clone

    |

    +----------------+

                     |

                     v

                  child

                     |

                     v

                  execve

                     |

                     v

                  build tool

                     |

                     v

                  exit


parent

    |

    v

wait4

    |

    v

collect result
```

---

# 15.5 execve requirements

execve requires:

```
program path

argv[]

envp[]
```

The first implementation should use a configured executable path.

Example:

```
/bin/make
```

with:

```
argv[0] = make

argv[1] = target
```

Do not introduce shell parsing initially.

A shell adds another layer:

```
editor

 |

shell

 |

command

 |

program
```

The first build launcher should execute directly.

---

# 15.6 wait4 result handling

The parent needs to decode the child result.

Possible outcomes:

```
success

failure exit code

terminated by signal
```

The process wrapper converts kernel results into:

```
ProcessResult

    exit_status

    termination_reason

    signal
```

The editor displays the result.

---

# 15.7 Build output capture

The first milestone must choose one method.

REMEDIT v1 uses temporary-file capture unless pipes are verified.

Workflow:

```
create temporary output file

        |

        v

launch build

        |

        v

wait for completion

        |

        v

read output file

        |

        v

delete temporary file
```

Ownership:

```
editor owns temporary file

cleanup occurs on success or failure
```

Future versions may replace this with pipe capture.

---

# Chapter 16 — Error Navigation and Build Integration (Revision 3)

Build output is converted into structured information.

---

# 16.1 Error model

```
BuildError

    filename

    line

    column

    severity

    message
```

Example:

```
main.S:42:7: error: unknown instruction
```

becomes:

```
filename = main.S

line = 42

column = 7

severity = error
```

---

# 16.2 Parser rules

The parser must handle:

## Compiler/assembler errors

```
file:line:column: message
```

## Warnings

Warnings are stored separately:

```
severity = warning
```

## Linker errors

Some messages have no source location:

```
undefined reference
```

These become:

```
filename = none

line = none

column = none
```

## Malformed messages

Unknown output remains available as:

```
unparsed build output
```

The editor must not lose diagnostics because parsing failed.

---

# 16.3 Error list

The editor stores:

```
Errors[]

    error 0

    error 1

    warning 0
```

The user navigates between entries.

---

# 16.4 Complete build workflow

```
edit

 |

 v

save

 |

 v

start process

 |

 v

wait

 |

 v

collect status

 |

 v

parse diagnostics

 |

 v

display errors

 |

 v

jump to source
```

---

# Tests

## process-test

Verify:

```
clone

execve

wait4
```

lifecycle.

---

## build-test

Verify:

Good source:

```
exit status = success
```

Bad source:

```
exit status = failure
```

---

## capture-test

Verify:

- output collected;
- temporary file removed;
- failure output preserved.

---

## error-parser-test

Input:

```
main.S:42:7: error: unknown instruction
```

Expected:

```
file = main.S

line = 42

column = 7

severity = error
```

---

Status:

```
Host QEMU:

test required


Fold:

validate separately
```
