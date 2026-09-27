# Kilo Editor Guide

Kilo is a small terminal editor that writes VT100-compatible escape sequences directly. It is useful on REM because it has no curses dependency.

## Start and open a file

```sh
kilo /root/projects/hello.s
```

Kilo creates a new buffer when the file does not exist. The status bar shows the filename and modified state.

## Editing

Type text directly. Use the arrow keys to move. Backspace deletes before the cursor. Kilo supports line insertion and editing in the current buffer.

Common commands:

- `Ctrl-S` — save the current file.
- `Ctrl-Q` — quit; if the buffer is modified, follow Kilo's confirmation behavior.
- `Ctrl-F` — search text, then press Enter to accept the search.
- Arrow keys — move through the buffer.
- Page movement — use the supported Page Up/Page Down keys.

**VERIFIED:** Kilo has passed edit, save, exit, reopen, and reboot-persistence testing on REM. Terminal geometry is supplied as 24 rows by 80 columns in the validated kernel.

## Typical assembly workflow

```sh
kilo hello.s
# edit, Ctrl-S, Ctrl-Q
as hello.s -o hello.o
ld -T /root/native/lib/myemulator2-user.ld -o hello hello.o
./hello
```

If the screen is garbled after an interrupted editor, run `reset` if that utility is present, or leave and re-enter the terminal session.
