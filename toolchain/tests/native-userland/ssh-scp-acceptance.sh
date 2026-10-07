#!/bin/bash
set -eu
case $0 in */*) cd "${0%/*}/.." ;; esac
source ./native-env.sh
: "${SSH_TEST_HOST:?Set SSH_TEST_HOST to the isolated test server}"
: "${SSH_TEST_PORT:?Set SSH_TEST_PORT}"
: "${SSH_TEST_USER:?Set SSH_TEST_USER}"
: "${SSH_TEST_KEY:?Set SSH_TEST_KEY to a private test identity}"
: "${SSH_TEST_KNOWN_HOSTS:?Set SSH_TEST_KNOWN_HOSTS to a pinned known_hosts file}"
: "${SSH_TEST_SOURCE:?Set SSH_TEST_SOURCE to the remote payload path}"
mkdir -p native-tests
packages/dropbear-2024.86/native/bin/scp \
    -S "$ROOT/packages/dropbear-2024.86/native/bin/dbclient" \
    -P "$SSH_TEST_PORT" -i "$SSH_TEST_KEY" \
    -o "UserKnownHostsFile=$SSH_TEST_KNOWN_HOSTS" \
    -o StrictHostKeyChecking=yes -o BatchMode=yes \
    "$SSH_TEST_USER@$SSH_TEST_HOST:$SSH_TEST_SOURCE" \
    "$ROOT/native-tests/ssh-scp-downloaded"
cmp tests/ssh-scp-payload.txt native-tests/ssh-scp-downloaded
echo SSH_SCP_ACCEPTANCE_OK
