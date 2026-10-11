# Owner artifact intake

Place delivered files in the matching directory (`applications`, `binutils`, `networking`,
`chibicc-samurai`, `yash`, or `offline-kit`). Keep delivered filenames and
their owner-provided acceptance evidence. Staging is not acceptance and does
not change the selected flight tuple.

For each overlay record owner, provenance, source, guest destination, exact
size, mode, SHA-256, dependencies, and expected ELF machine where applicable.
Set `accepted: true` only after acceptance passes on the candidate. To run an
acceptance trial before promotion, set `accepted: false` and
`candidate_test_only: true`; the output manifest marks the candidate TEST ONLY
and it must not be selected for flight.

Kilo is staged under `applications/kilo` for candidate-only testing. Its
candidate acceptance failed, so its overlay remains unaccepted. The current
qualified `ar` and `ranlib` files are staged under `binutils/` with hashes from
the binutils owner qualification; the reviewed overlay also removes hash-pinned
stale tools from the alternate prefix. Do not infer
acceptance from any artifact's presence in intake.

`networking/git` is a Git 2.48.1 intake artifact with a previous exact-image
`git --version` PASS. It has no retained compiler/build-flag record and has not
been tested in the current distribution or for network transports. Its
provenance is in `networking/git.provenance.json`; it is not accepted for the
current candidate. Curl and SSH are withheld because the available REM client
artifacts have recorded runtime faults. See
[`REM_NETWORK_CLIENTS.md`](../../../docs/REM_NETWORK_CLIENTS.md).

## Pending unified flight selection

`chibicc-samurai/chibicc-e01b58f` and `chibicc-samurai/samu-d933c9e` are
hash-pinned Codex #2 deliveries. The owner reports `samu -h` PASS; `samu -n`
and incremental rebuild remain blocking tests. Do not build a candidate from
the pending overlay until both pass. `yash/yash`, the Kilo executable, licensed
Kilo source, and its guides are staged for the selected Yash tuple. Their
same-candidate acceptance status is recorded in `../READINESS.md`.

`networking/remfetch` is a separate static HTTP-only candidate for the
checksum-qualified network kernel. Its SHA-256 is
`b578bf04342e1f461a06949340a1c9135bfbb422b8ac1a6095a3b9dbc05579ee`; source,
build tuple, fixture test, and limitations are in
`networking/remfetch.provenance.json`. It passed a disposable HTTP fixture
download and ext4 readback, using a `sync()` fallback because fsync syscall 82
returns `ENOSYS` on that kernel. It is not installed in a release image.

Do not promote the retained curl or Git binaries based on version output:
curl faults even on `--version`, and Git's `git init` faults after
`git --version` succeeds. See `../../../docs/REM_NETWORK_CLIENTS.md` for exact
results and diagnostic records.
