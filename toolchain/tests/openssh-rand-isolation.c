/* Same source is built with REM GCC and REM chibicc to isolate the OpenSSL
 * startup sequence used by OpenSSH seed_rng(). */
extern int puts(char *);
typedef unsigned long long rem_u64;
extern int OPENSSL_init_crypto(rem_u64, void *);
extern int RAND_status(void);

int main(void)
{
	int result;
	puts("RAND_PROBE_BEFORE_INIT");
	result = OPENSSL_init_crypto((rem_u64)12, (void *)0);
	puts(result == 1 ? "RAND_PROBE_INIT_PASS" : "RAND_PROBE_INIT_FAIL");
	puts("RAND_PROBE_BEFORE_STATUS");
	result = RAND_status();
	puts(result == 1 ? "RAND_PROBE_STATUS_PASS" : "RAND_PROBE_STATUS_FAIL");
	return result == 1 ? 0 : 1;
}
