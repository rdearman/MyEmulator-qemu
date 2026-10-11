/* Isolate OpenSSL's REM lock allocation/cleanup path from ssh-keygen. */
#include <openssl/crypto.h>
#include <stdint.h>
#include <stdio.h>

int
main(void)
{
	CRYPTO_RWLOCK *lock;
	uintptr_t address;

	if (OPENSSL_init_crypto(0, NULL) != 1) {
		puts("LOCK_PROBE_INIT_FAIL");
		return 1;
	}
	lock = CRYPTO_THREAD_lock_new();
	address = (uintptr_t)lock;
	printf("LOCK_PROBE_PTR=%08lx MOD4=%lu\n",
	    (unsigned long)address, (unsigned long)(address & 3));
	if (lock == NULL)
		return 2;
	if (!CRYPTO_THREAD_write_lock(lock)) {
		puts("LOCK_PROBE_WRITE_FAIL");
		return 3;
	}
	if (!CRYPTO_THREAD_unlock(lock)) {
		puts("LOCK_PROBE_UNLOCK_FAIL");
		return 4;
	}
	CRYPTO_THREAD_lock_free(lock);
	puts("LOCK_PROBE_PASS");
	return 0;
}
