#!/usr/bin/env python3
"""REM32 HALT/IRQ/RFE regression on a disposable R4-compatible rootfs.

Usage:
  idle-wakeup-stress.py QEMU KERNEL ROOTFS CONSOLE_CLIENT [--cycles N]

The test boots the real SysV image, runs a sleeping child repeatedly so the
guest enters and leaves HALT through timer interrupts, verifies the shell and
stack-sensitive child execution after wakeups, tests guest Ctrl-C, then uses
the existing dev sudo permission for a clean shutdown. ROOTFS MUST be a copy.
"""
import argparse
import os
import pexpect
import subprocess
import time


def check_translator(path):
    text = open(path, encoding="utf-8").read()
    marker = "if (sysop == 3) {"
    start = text.index(marker)
    end = text.index("gen_helper_halt(tcg_env);", start) + len("gen_helper_halt(tcg_env);")
    body = text[start:end]
    ordered = [body.index(f"tcg_gen_st_i32({reg}") for reg in
               ("cpu_sr", "cpu_r[13]", "cpu_r[15]")]
    assert ordered == sorted(ordered), "HALT publication order/content changed"
    assert body.index("cpu_r[15]") < body.index("gen_next_pc(ctx)")
    print("HALT_TRANSLATOR_STATE_PUBLICATION=PASS", flush=True)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("qemu")
    ap.add_argument("kernel")
    ap.add_argument("rootfs")
    ap.add_argument("console_client")
    ap.add_argument("--translator", default="qemu/target/myemulator32/translate.c")
    ap.add_argument("--cycles", type=int, default=1921)
    ap.add_argument("--rows", type=int, default=42)
    ap.add_argument("--cols", type=int, default=132)
    args = ap.parse_args()
    if args.cycles < 1:
        ap.error("--cycles must be positive")
    check_translator(args.translator)
    sock = args.rootfs + ".idle-wakeup.sock"
    try:
        os.unlink(sock)
    except FileNotFoundError:
        pass
    append = (f"console=ttyMY0 115200 root=/dev/myemu0 rw init=/sbin/init "
              f"myemu.tty_rows={args.rows} myemu.tty_cols={args.cols} ip=off "
              "virtio_mmio.device=0x200@0xf0200000:5")
    qcmd = [args.qemu, "-M", "myemulator32,append=" + append, "-m", "128M",
            "-kernel", args.kernel, "-drive",
            f"file={args.rootfs},format=raw,if=none,id=myemulator2-disk,file.locking=off",
            "-netdev", "user,id=net0", "-display", "none", "-chardev",
            f"socket,id=remconsole,path={sock},server=on,wait=off", "-serial",
            "chardev:remconsole", "-monitor", "none", "-icount",
            "shift=0,sleep=off", "-no-reboot"]
    qlog = open("idle-wakeup-stress-qemu.log", "w", buffering=1)
    q = subprocess.Popen(qcmd, stdin=subprocess.DEVNULL,
                         stdout=qlog, stderr=subprocess.STDOUT,
                         start_new_session=True)
    client = None
    try:
        deadline = time.monotonic() + 120
        while not os.path.exists(sock):
            if q.poll() is not None:
                raise RuntimeError(f"QEMU exited during boot: {q.returncode}")
            if time.monotonic() > deadline:
                raise RuntimeError("serial socket did not appear")
            time.sleep(.1)
        client = pexpect.spawn(args.console_client, [sock], encoding="utf-8",
                               timeout=60, dimensions=(args.rows, args.cols),
                               env=dict(os.environ, TERM="ansi"))
        client.expect("login:")
        client.sendline("dev")
        client.expect("Password:")
        client.sendline("remtest")
        client.expect("REM Linux", timeout=60)
        client.sendline("printf 'HALT_WAKE_%s\\n' LOGIN_PASS; stty size")
        client.expect_exact("HALT_WAKE_LOGIN_PASS", timeout=30)
        print("BOOT_LOGIN=PASS", flush=True)
        completion_a = 193 + args.cycles
        completion_b = 197 + args.cycles
        completion_mark = completion_a * completion_b + completion_a
        loop = (f"i=0; while [ $i -lt {args.cycles} ]; do sleep 1; "
                "i=$((i+1)); done; "
                f"printf '%s\\n' \"$(({completion_a}*{completion_b}+{completion_a}))\"")
        client.sendline(loop)
        client.expect_exact(str(completion_mark), timeout=args.cycles + 180)
        shell_mark = 211 + args.cycles
        shell_b = 223 + args.cycles
        shell_result = shell_mark * shell_b + shell_mark
        client.sendline(f"printf '%s\\n' \"$(({shell_mark}*{shell_b}+{shell_mark}))\"")
        client.expect_exact(str(shell_result), timeout=30)
        client.sendline("/bin/echo AFTER_WAKE_EXEC_PASS; /bin/true; echo $? ")
        client.expect_exact("AFTER_WAKE_EXEC_PASS", timeout=30)
        client.expect_exact("0", timeout=30)
        if "AFTER_WAKE_EXEC_PASS" not in client.before:
            raise RuntimeError("post-wakeup external command did not return expected output")
        print(f"HALT_IRQ_RFE_WAKEUPS={args.cycles}:PASS", flush=True)
        client.sendline("sleep 1000")
        time.sleep(1)
        client.send("\x03")
        interrupt_mark_a = 227 + args.cycles
        interrupt_mark_b = 229 + args.cycles
        interrupt_mark = interrupt_mark_a * interrupt_mark_b + interrupt_mark_a
        client.sendline(f"printf '%s\\n' \"$(({interrupt_mark_a}*{interrupt_mark_b}+{interrupt_mark_a}))\"")
        client.expect_exact(str(interrupt_mark), timeout=30)
        print("GUEST_CTRL_C=PASS", flush=True)
        client.sendline("sudo -i")
        client.expect("password for dev:", timeout=45)
        client.sendline("remtest")
        client.expect("root@myemulator2", timeout=45)
        client.sendline("sync; poweroff")
        client.expect(pexpect.EOF, timeout=120)
        client.close()
        rc = q.wait(timeout=30)
        if rc != 0:
            raise RuntimeError(f"orderly QEMU shutdown returned {rc}")
        print("CLEAN_SHUTDOWN=PASS", flush=True)
    finally:
        if client is not None and client.isalive():
            client.close(force=True)
        if q.poll() is None:
            q.terminate()
            try:
                q.wait(timeout=5)
            except subprocess.TimeoutExpired:
                q.kill()
                q.wait()
        qlog.close()
        try:
            os.unlink(sock)
        except FileNotFoundError:
            pass


if __name__ == "__main__":
    main()
