#!/bin/sh
# Run only as root from the one-time maintenance shell.
set -eu
identity=$(/bin/id 2>/dev/null || id 2>/dev/null || true)
case "$identity" in
  'uid=0('*|'uid=0 '*) ;;
  *) echo "admin update must run as root" >&2; exit 1 ;;
esac
case "$0" in
  */*) here=${0%/*}; [ -n "$here" ] || here=/ ;;
  *) here=. ;;
esac
[ -x "$here/sudo.new" ] || { echo "missing sudo.new" >&2; exit 2; }
[ -r /etc/shadow ] || { echo "missing /etc/shadow" >&2; exit 2; }
grep '^root:x:0:0:' /etc/passwd >/dev/null 2>&1 && grep '^dev:x:1000:100:' /etc/passwd >/dev/null 2>&1 || {
  echo "expected root UID 0 and dev UID 1000; refusing account rewrite" >&2; exit 1;
}
backup() {
  src=$1
  [ -e "$src" ] || return 0
  dst="$src.pre-rem-admin.$$"
  [ ! -e "$dst" ] || { echo "backup already exists: $dst" >&2; exit 1; }
  cp "$src" "$dst"
  chown 0:0 "$dst"
  case "$src" in
    /etc/shadow) chmod 0600 "$dst" ;;
    /etc/sudoers) chmod 0440 "$dst" ;;
    /usr/bin/sudo) chmod 0755 "$dst" ;;
    *) chmod 0644 "$dst" ;;
  esac
  echo "backup: $dst"
}
backup /etc/shadow
backup /etc/securetty
backup /etc/sudoers
backup /usr/bin/sudo
root_entry=$(grep '^root:' "$here/accounts-shadow" || true)
root_hash=${root_entry#root:}
root_hash=${root_hash%%:*}
dev_entry=$(grep '^dev:' "$here/accounts-shadow" || true)
dev_hash=${dev_entry#dev:}
dev_hash=${dev_hash%%:*}
[ -n "$root_hash" ] && [ -n "$dev_hash" ] || { echo "root/dev hashes missing from update" >&2; exit 2; }
shadow_tmp=/etc/.shadow.rem-admin.$$
grep '^root:' /etc/shadow >/dev/null 2>&1 || { echo "root account missing from /etc/shadow" >&2; exit 1; }
grep '^dev:' /etc/shadow >/dev/null 2>&1 || { echo "dev account missing from /etc/shadow" >&2; exit 1; }
sed -e "s|^root:[^:]*:|root:$root_hash:|" \
    -e "s|^dev:[^:]*:|dev:$dev_hash:|" /etc/shadow > "$shadow_tmp"
chown 0:0 "$shadow_tmp"
chmod 0600 "$shadow_tmp"
mv "$shadow_tmp" /etc/shadow
sudo_tmp=/usr/bin/.sudo.rem-admin.$$
cp "$here/sudo.new" "$sudo_tmp"
chown 0:0 "$sudo_tmp"
chmod 4755 "$sudo_tmp"
mv "$sudo_tmp" /usr/bin/sudo
[ -e /etc/securetty ] || : > /etc/securetty
for tty in console ttyMY0 UNKNOWN; do
  grep "^$tty\$" /etc/securetty >/dev/null 2>&1 || printf '%s\n' "$tty" >> /etc/securetty
done
chown 0:0 /etc/securetty
chmod 0644 /etc/securetty
[ -e /etc/sudoers ] || : > /etc/sudoers
grep '^root ALL=(ALL) ALL$' /etc/sudoers >/dev/null 2>&1 || printf '%s\n' 'root ALL=(ALL) ALL' >> /etc/sudoers
grep '^dev ALL=(ALL) ALL$' /etc/sudoers >/dev/null 2>&1 || printf '%s\n' 'dev ALL=(ALL) ALL' >> /etc/sudoers
chown 0:0 /etc/sudoers
chmod 0440 /etc/sudoers
sync
printf 'Installed sudo: '
ls -l /usr/bin/sudo
printf 'Root password set to remdev and dev password set to remtest; /home/dev was preserved.\n'
