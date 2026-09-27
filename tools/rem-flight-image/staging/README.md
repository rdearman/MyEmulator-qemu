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

## Pending unified flight selection

`chibicc-samurai/chibicc-e01b58f` and `chibicc-samurai/samu-d933c9e` are
hash-pinned Codex #2 deliveries. The owner reports `samu -h` PASS; `samu -n`
and incremental rebuild remain blocking tests. Do not build a candidate from
the pending overlay until both pass. `yash/yash`, the Kilo executable, licensed
Kilo source, and its guides are staged for the selected Yash tuple. Their
same-candidate acceptance status is recorded in `../READINESS.md`.
