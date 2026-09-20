# Initial static-only port.  The ABI and syscall definitions are supplied by
# the MyEmulator2 overlay; dynamic linking is intentionally deferred.  The
# upstream Makefile already adds arch/$(ARCH) to CFLAGS_ALL; do not define
# ARCH_INCLUDES here because that name is also used for exported headers.
