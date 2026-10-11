#!/usr/bin/env python3
"""Run the REM ptrace/debugger smoke suite in a disposable guest image.

The rootfs must contain the debugger and fixture paths below. The fixture is
intentionally built with the REM toolchain; see docs/REM_DEBUGGER.md.
"""

import os
import re
import signal
import sys
import time

import pexpect


qemu = os.environ.get("REM_QEMU", ".qemu-build/qemu-system-myemulator32")
kernel = os.environ.get("REM_KERNEL", "/tmp/remdbg-kernel")
rootfs = os.environ.get("REM_ROOTFS")
log_path = os.environ.get("REMDBG_LOG", "/tmp/remdbg-qualification.log")
guest_debugger = os.environ.get("REMDBG_GUEST_DEBUGGER", "/remdbg")
guest_program = os.environ.get("REMDBG_GUEST_PROGRAM", "/debug_test")
if not rootfs:
    sys.exit("set REM_ROOTFS to a disposable ext4 image with the debugger and fixture")


def qemu_processes():
    expected = os.path.basename(qemu)
    found = []
    for entry in os.listdir("/proc"):
        if not entry.isdigit():
            continue
        proc = f"/proc/{entry}"
        try:
            executable = os.path.basename(os.readlink(f"{proc}/exe"))
            if executable != expected:
                continue
            with open(f"{proc}/cmdline", "rb") as cmdline_file:
                command_line = cmdline_file.read().replace(b"\0", b" ").decode(
                    "utf-8", errors="replace").strip()
            found.append(f"PID={entry} COMMAND={command_line}")
        except (FileNotFoundError, PermissionError, ProcessLookupError):
            continue
    return found


already_running = qemu_processes()
if already_running:
    sys.exit("refusing to launch duplicate QEMU; existing process(es):\n" +
             "\n".join(already_running))

argv = [
    "-M", "myemulator32", "-m", os.environ.get("REM_MEMORY", "16M"), "-kernel", kernel,
    "-drive", f"file={rootfs},format=raw,if=none,id=myemulator2-disk",
    "-nographic", "-monitor", "none", "-serial", "stdio",
    "-icount", "shift=0,sleep=off",
]
kernel_cmdline = os.environ.get("REM_KERNEL_CMDLINE")
if kernel_cmdline:
    argv += ["-append", kernel_cmdline]
runner = os.path.join(os.path.dirname(__file__), "qemu-tracked.sh")
qemu_record = os.environ.get("REM_QEMU_RECORD", log_path + ".qemu")
qemu_purpose = os.environ.get("REM_QEMU_PURPOSE", "REM debugger integration qualification")
os.makedirs(os.path.dirname(os.path.abspath(qemu_record)), exist_ok=True)
try:
    os.unlink(qemu_record)
except FileNotFoundError:
    pass
guest = pexpect.spawn(runner, [qemu_record, qemu_purpose, qemu, *argv], timeout=180, encoding="utf-8",
                      codec_errors="replace")
guest.logfile = open(log_path, "w", encoding="utf-8")
qualification_passed = False

# The launcher writes the exact PID/argv before exec replaces it with QEMU.
for _ in range(200):
    if os.path.isfile(qemu_record):
        break
    time.sleep(0.01)
if not os.path.isfile(qemu_record):
    guest.close(force=True)
    sys.exit(f"QEMU launch record was not written: {qemu_record}")
with open(qemu_record, encoding="utf-8") as record_file:
    record = record_file.read()
if f"PID={guest.pid}\n" not in record or "COMMAND=" not in record:
    guest.close(force=True)
    sys.exit(f"QEMU launch record does not match child PID {guest.pid}: {qemu_record}")


def command(line, expected, timeout=40):
    guest.sendline(line)
    guest.expect(expected, timeout=timeout)


def wait_for_shell(timeout=30):
    # Some REM rootfs init paths run an interactive Bash without a controlling
    # terminal. It accepts commands but may not emit a parseable prompt.
    guest.sendline("echo REMDBG_SHELL_READY")
    guest.expect(r"REMDBG_SHELL_READY", timeout=timeout)


def self_step_case(arg, symbol, mnemonic, check_link=False):
    command(f"{guest_debugger} {guest_program} {arg}", r"remdbg> ", 30)
    command(f"break {symbol}", r"breakpoint 1 at")
    guest.expect(r"remdbg> ")
    command("run", r"hit breakpoint at", 40)
    guest.expect(r"remdbg> ")
    branch_pc = None
    for _ in range(32):
        guest.sendline("disasm")
        guest.expect(r"remdbg> ")
        disasm_output = guest.before.replace("\r", "")
        current = re.search(r"(?m)^0x([0-9a-fA-F]{8}):\s+[0-9a-fA-F]{8}\s+(.+)$", disasm_output)
        transfer = re.search(rf"^{mnemonic} 0x([0-9a-fA-F]{{8}})", current.group(2)) if current else None
        if transfer and current.group(1).lower() == transfer.group(1).lower():
            branch_pc = int(current.group(1), 16)
            break
        guest.sendline("step")
        guest.expect(r"remdbg> ")
        if "step failed" in guest.before:
            raise AssertionError(f"single-step failed before reaching {symbol}")
    if branch_pc is None:
        raise AssertionError(f"did not find self-targeting {mnemonic} in {symbol}")
    guest.sendline("step")
    guest.expect(r"remdbg> ")
    if "step failed" in guest.before:
        raise AssertionError(f"single-step rejected self-targeting {mnemonic}")
    guest.sendline("regs")
    guest.expect(r"remdbg> ")
    regs_output = guest.before
    pc_match = re.search(r"\bpc=([0-9a-fA-F]{8})", regs_output)
    if not pc_match or int(pc_match.group(1), 16) != branch_pc:
        raise AssertionError(f"self-targeting {mnemonic} step did not stop at its PC")
    if check_link:
        lr_match = re.search(r"\blr=r14=([0-9a-fA-F]{8})", regs_output)
        if not lr_match or int(lr_match.group(1), 16) != branch_pc + 4:
            raise AssertionError("self-targeting JAL did not expose the updated LR")
    guest.sendline("quit")
    wait_for_shell(20)


try:
    initial_prompt = os.environ.get("REMDBG_INITIAL_PROMPT_REGEX", r"# ")
    guest.expect(initial_prompt, timeout=150)

    pre_test_command = os.environ.get("REMDBG_PRE_TEST_COMMAND")
    if pre_test_command:
        guest.sendline(pre_test_command)
        pre_test_marker = os.environ.get(
            "REMDBG_PRE_TEST_MARKER", "REM_DEVELOPER_BUILD_WORKFLOW_PASS")
        markers = [r"(?m)^" + re.escape(pre_test_marker) + r"\r?$"]
        failure_marker = os.environ.get(
            "REMDBG_PRE_TEST_FAIL_MARKER", "REM_DEVELOPER_BUILD_WORKFLOW_FAIL")
        if failure_marker:
            markers.append(r"(?m)^" + re.escape(failure_marker) + r"\r?$")
        outcome = guest.expect(markers, timeout=300)
        if outcome:
            raise AssertionError("developer workflow smoke script reported failure")
        wait_for_shell(30)

        # Focused reducers can use the same tracked guest lifecycle without
        # falling through into the unrelated remdbg qualification suite.
        if os.environ.get("REMDBG_PRE_TEST_ONLY") == "1":
            print(pre_test_marker)
            qualification_passed = True
            raise SystemExit(0)

    command(f"{guest_debugger} {guest_program}", r"remdbg> ", 30)
    command(f"load {guest_program}",
            rf"loaded {re.escape(guest_program)} \(pid \d+\); initial exec stop", 30)
    guest.expect(r"remdbg> ")
    guest.sendline("break main")
    guest.expect(r"breakpoint 1 at (0x[0-9a-fA-F]+)")
    main_address = guest.match.group(1)
    guest.expect(r"remdbg> ")
    command("run", r"hit breakpoint at")
    guest.expect(r"remdbg> ")
    guest.sendline("regs")
    guest.expect(r"remdbg> ")
    regs_output = guest.before
    argv_match = re.search(r"\br2\s*=([0-9a-fA-F]{8})", regs_output)
    if not argv_match:
        raise AssertionError("could not read main's argv pointer from regs")
    # Write back argv[0]'s existing leading '/' byte.  This exercises
    # PTRACE_PEEKDATA/POKEDATA without changing the program's behavior.
    command(f"write 0x{argv_match.group(1)} 0x2f", r"memory updated")
    guest.expect(r"remdbg> ")
    command("bt", r"#0 .*main")
    guest.expect(r"remdbg> ")
    if os.environ.get("REMDBG_DWARF") == "1":
        guest.sendline("source")
        guest.expect(r"debug_test\.c:\d+", timeout=60)
        guest.expect(r"remdbg> ")
        guest.sendline("locals")
        guest.expect(r"argc = 0x00000001 \(entry argument r1\)", timeout=60)
        guest.expect(r"argv = 0x[0-9a-fA-F]+ \(entry argument r2\)")
        guest.expect(r"remdbg> ")
        guest.sendline("globals")
        guest.expect(r"result = 0x00000000 @0x[0-9a-fA-F]{8} \(DW_OP_addr, 32-bit word\)", timeout=60)
        guest.expect(r"remdbg> ")
    command(f"mem {main_address} 8", rf"{main_address[2:]}:")
    guest.expect(r"remdbg> ")
    command("disasm", r"subi r13")
    guest.expect(r"remdbg> ")
    command("step", r"pc symbol: main\+0x4")
    guest.expect(r"remdbg> ")
    if os.environ.get("REMDBG_DWARF") == "1":
        command("break add_values", r"breakpoint 2 at")
        guest.expect(r"remdbg> ")
        command("continue", r"hit breakpoint at", 50)
        guest.expect(r"remdbg> ")
        guest.sendline("locals")
        guest.expect(r"left = 0x00000013 \(entry argument r1\)", timeout=60)
        guest.expect(r"right = 0x00000017 \(entry argument r2\)")
        guest.expect(r"local: DW_OP_fbreg -20 \(value unavailable: target location not validated\)")
        guest.expect(r"remdbg> ")
        command("continue", r"stopped: signal 5", 50)
        guest.expect(r"remdbg> ")
        guest.sendline("globals")
        guest.expect(r"result = 0x0000002a @0x[0-9a-fA-F]{8} \(DW_OP_addr, 32-bit word\)", timeout=60)
        guest.expect(r"remdbg> ")
        guest.sendline("continue")
        guest.expect(r"skipping explicit BREAK instruction at")
        guest.expect(r"program exited with status 0", timeout=50)
    else:
        command("continue", r"program exited with status 0", 50)
    wait_for_shell(20)

    if os.environ.get("REMDBG_SELF_BRANCH") == "1":
        # Self-targeting control flow must single-step to the same PC. A
        # linking jump must also expose its architectural LR write.
        self_step_case("self-step", "self_branch", "j")
        self_step_case("self-link-step", "self_link_branch", "jal", check_link=True)

    # Re-running the same ELF catches stale software-breakpoint patches.  The
    # first continue steps over main's breakpoint, then must expose the
    # deliberate SIGSEGV with a useful fault PC and stack state.
    command(f"{guest_debugger} {guest_program} fault", r"remdbg> ", 30)
    command("break main", r"breakpoint 1 at")
    guest.expect(r"remdbg> ")
    command("run", r"hit breakpoint at")
    guest.expect(r"remdbg> ")
    command("continue", r"stopped: signal 11", 50)
    if os.environ.get("REMDBG_DWARF") == "1":
        fault_frame = "deliberate_fault"
        guest.expect(rf"pc symbol: {fault_frame}\+")
    else:
        guest.expect(r"pc symbol: (deliberate_fault|main)\+")
        fault_frame = guest.match.group(1)
    guest.expect(r"remdbg> ")
    command("regs", r"pc=")
    guest.expect(r"remdbg> ")
    command("bt", rf"#0 .*{fault_frame}")
    guest.expect(r"#1 .*main" if fault_frame == "deliberate_fault"
                 else r"#1 .*libc_start_main")
    guest.expect(r"remdbg> ")
    command("continue", r"program terminated by signal 11", 30)
    wait_for_shell(20)

    print("REMDBG_BREAK_STEP_CONTINUE_MEMORY_DISASM_BT_FAULT_PASS")
    qualification_passed = True
finally:
    try:
        if guest.isalive():
            # The successful path has returned to the root shell. Ask the guest
            # to shut down cleanly before terminating QEMU as a fallback.
            if qualification_passed:
                guest.sendline("poweroff")
                try:
                    guest.expect(pexpect.EOF, timeout=15)
                except pexpect.TIMEOUT:
                    pass
            if guest.isalive():
                guest.kill(signal.SIGTERM)
                try:
                    guest.expect(pexpect.EOF, timeout=5)
                except pexpect.TIMEOUT:
                    guest.kill(signal.SIGKILL)
        guest.close(force=False)
    except Exception:
        try:
            if guest.isalive():
                guest.kill(signal.SIGKILL)
            guest.close(force=True)
        except Exception:
            pass

remaining = qemu_processes()
if remaining:
    sys.exit("QEMU process(es) remain after qualification cleanup:\n" +
             "\n".join(remaining))
