# Devices and Peripherals

No peripheral is required for the initial CPU milestone. QEMU currently
provides one CPU and flat 64 KiB RAM, including reset vectors.

The old Python CLI queue and its POP bit-7 services (`SYS_EXIT`, `SYS_PRINT`,
and `SYS_UNAME`) are historical host conveniences, not current hardware. The
new architecture assigns no syscall meaning to POP or `0xf080`; the latter is
HALT. Console, storage, timers, interrupts, and banking will be designed as
real QEMU devices when their interfaces are defined.

The old `harddrive` directory, monitor commands, and direct RAM inspection are
development tooling only, not guest-visible devices.
