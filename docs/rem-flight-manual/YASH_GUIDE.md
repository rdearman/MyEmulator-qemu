# Yash Guide

Yash is the preferred interactive shell when installed as `/usr/bin/yash`. BusyBox `ash` remains the recovery shell.

## Commands and variables

```sh
echo hello
pwd
cd /root
export PROJECT=/root/projects
echo "$PROJECT"
name=REM
echo "$name"
```

Set the normal environment for a flight session:

```sh
export PATH=/root/native/bin:/usr/bin:/bin:/usr/sbin:/sbin
export HOME=/root
export TERM=xterm
```

## Quoting and expansion

Single quotes preserve literal text; double quotes expand variables; `$()` captures command output:

```sh
echo 'literal $HOME'
echo "home is $HOME"
now=$(pwd)
echo "$now"
```

## Aliases and functions

```sh
alias ll='ls -l'
ll
build() { chibicc -o "$1" "$1.c"; }
```

The exact startup-file name depends on the Yash installation. **PLANNED:** place stable aliases and exports in the documented Yash startup file after confirming it exists in the image.

## Pipes and redirects

```sh
ls | grep hello
readelf -h hello > hello.header
objdump -d hello >> disassembly.txt
command 2> errors.txt
command > output.txt 2>&1
```

## Scripts

Create an executable script:

```sh
cat > build.sh <<'EOF2'
#!/bin/yash
set -e
chibicc -o hello hello.c
./hello
EOF2
chmod +x build.sh
./build.sh
```

## History and editing

**VERIFIED:** Yash uses `pselect6` while waiting for input and supports interactive line editing in the validated environment. Use Up/Down for history, Left/Right to move, Backspace to erase, and Enter to execute. **FUTURE:** persistent history behavior depends on the installed startup configuration and should be checked before relying on it.

## Job control

**PLANNED/ENVIRONMENT-DEPENDENT:** Where the terminal image provides complete process-group control:

```sh
long-command &
jobs
fg
```

Ctrl-C interrupts a foreground command when the shell owns the terminal's foreground process group. If job control is unavailable, use a separate shell or terminate the process explicitly; this is a terminal-image limitation rather than a shell-language feature.

## Exit

```sh
exit
```

Use `Ctrl-D` at an empty prompt where supported.
