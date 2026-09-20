# Initial static-only port.  The ABI and syscall definitions are supplied by
# the MyEmulator2 overlay; dynamic linking is intentionally deferred.
ARCH_INCLUDES = -I$(srcdir)/arch/myemulator2
