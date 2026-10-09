# REM Package Format (0.2)

REM 0.2 uses data-only JSON manifests and install trees. It does not add a
package manager. The host assembler verifies payload sizes and SHA-256 values,
copies files into a staging tree, writes the installed package records, then
builds an ext4 image.

Each manifest records a stable package name/version, `myemulator32`
architecture, dependencies, source/build provenance, and a sorted file list.
Each regular file entry contains an absolute guest path, mode, size, SHA-256,
and package-relative payload source. Package import rejects traversal paths,
duplicate guest paths, non-REM ELF files, and digest mismatches. Symlinks and
device nodes are currently produced by the assembler rather than imported
from package payloads. A `seed-provided` package may use a `seed:/absolute/path`
source for files already in the pinned migration image; the assembler verifies
each listed size and digest before recording it as installed.

For packages built from repository source, the installed manifest in the image
records the exact output file hashes, pinned seed hash, and compiler hash. The
source package definition also names the builder and source files. Pending
packages have `status: "pending"` and an empty file list; they are not silently
represented as installed.

The desktop and Fold profiles select the same shared package names and use the
same payload definitions. A later Fold-native install tree can replace a
package payload and update its provenance/hash records without changing the
distribution package identity or profile selection.

The first desktop migration uses the read-only seed pinned in
`distribution/manifests/desktop-seed.json`. This is a migration input, not a
source-built base. A clean checkout currently needs that local seed image;
the seed is not committed because it is a 1 GiB qualified image. Replacing it
with a source-built base is an explicit future change to the assembler input.
