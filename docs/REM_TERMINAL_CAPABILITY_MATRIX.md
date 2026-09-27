# Terminal capability boundary

This is a boundary map for offline development. “Documented” means the
interface is described in the book or references; “exercised” means a named
fixture or regression uses it. It does not claim a complete terminal emulator.

| Capability | Status | Evidence/boundary |
|---|---|---|
| byte input/output | documented, exercised | syscall and console fixtures |
| ANSI/CSI cursor and erase sequences | documented | `Inside_REM_Edition3.pdf`, Appendix E |
| SGR/reset and cursor visibility | documented | Appendix E |
| canonical/raw termios concepts | documented | Appendix R and worked terminal program |
| `VMIN`/`VTIME` | documented | Appendix R; verify libc headers for values |
| `TIOCGWINSZ` | documented | Appendix R; wrapper availability is target-dependent |
| resize signals | conceptual | signal support is documented; interactive delivery needs target verification |
| full VT220/ECMA-48 behaviour | outside scope | use `VT220_Programmer_Reference_Manual.pdf` |

Keep terminal protocol parsing in the terminal module. The editor should
consume events, not depend on escape-sequence spelling.
