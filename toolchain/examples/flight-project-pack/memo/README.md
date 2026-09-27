# memo

A small append-only notebook for logs, commands, and trip notes. Each record
has a tag and a text field, stored in a versioned, escaped line format. The
program can list all records or filter by a literal substring. A separately
compiled `note_store.c` is archived as `libnote.a`, demonstrating a reusable
static C library without a database server or external package.

## Build and use

On REM, this requires matching native `chibicc` and `ar`; `as`/`ld`, CRT,
musl, and `libgcc.a` are selected by the compiler driver's runtime
configuration.
This is Make-independent:

```sh
./build.sh
./memo add trip.remnote systems "Root mounts read-write"
./memo add trip.remnote tools "inspect the kernel with hexscope"
./memo list trip.remnote
./memo find trip.remnote kernel
```

On x86/ARM Linux, use `CC=cc AR=ar ./build.sh` for the portable source path.

When Samurai is accepted, the local `build.ninja` builds `libnote.a` first,
then links the application. Existing files with an unknown format are
refused; records are appended and the program never rewrites the notebook.

## Expected output

`./memo list trip.remnote` prints:

```text
systems: Root mounts read-write
tools: inspect the kernel with hexscope
```

`./memo find trip.remnote kernel` prints only the `tools` record.

The sample file contains these two records. The on-disk `REMNOTE1` header
and backslash escapes allow tabs and newlines in future records without
breaking the record boundary.
