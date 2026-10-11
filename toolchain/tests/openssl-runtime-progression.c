#include <stdio.h>
#include <stdlib.h>

/* Kept within the hosted chibicc subset: one-character dispatch and puts-only
 * markers make compiler call behavior less likely to confound the library test. */
typedef unsigned long long rem_u64;
typedef struct ossl_provider_st OSSL_PROVIDER;
extern int OPENSSL_init_crypto(rem_u64, const void *);
extern int OPENSSL_init_ssl(rem_u64, const void *);
extern int RAND_poll(void);
extern int RAND_status(void);
extern int RAND_bytes(unsigned char *, int);
extern void RAND_add(const void *, int, double);
extern OSSL_PROVIDER *OSSL_PROVIDER_load(void *, const char *);
extern unsigned long OpenSSL_version_num(void);

static int crypto_init(void)
{
	return OPENSSL_init_crypto((rem_u64)12, NULL) == 1;
}

int main(int argc, char **argv)
{
	int ok = 0;
	if (argc != 2 || argv[1][1] != '\0') return 64;
	puts(OpenSSL_version_num() != 0 ? "OPENSSL_VERSION_PASS" : "OPENSSL_VERSION_FAIL");
	if (argv[1][0] == '1') {
		puts("BEFORE_OPENSSL_INIT_CRYPTO");
		ok = crypto_init();
	} else if (argv[1][0] == '2') {
		puts("BEFORE_OPENSSL_INIT_CRYPTO");
		ok = crypto_init();
		if (ok) {
			puts("BEFORE_OPENSSL_INIT_SSL");
			ok = OPENSSL_init_ssl((rem_u64)0, NULL) == 1;
		}
	} else if (argv[1][0] == '3') {
		puts("BEFORE_RAND_POLL");
		ok = RAND_poll() == 1;
	} else if (argv[1][0] == '4') {
		puts("BEFORE_OPENSSL_INIT_CRYPTO");
		ok = crypto_init();
		if (ok) {
			puts("OPENSSL_INIT_CRYPTO_PASS");
			puts("BEFORE_RAND_STATUS");
			ok = RAND_status() == 1;
		}
	} else if (argv[1][0] == '7') {
		unsigned char bytes[32];
		puts("BEFORE_OPENSSL_INIT_CRYPTO");
		ok = crypto_init();
		if (ok) {
			puts("BEFORE_RAND_BYTES");
			ok = RAND_bytes(bytes, (int)sizeof(bytes)) == 1;
		}
		if (ok) {
			puts("BEFORE_RAND_STATUS");
			ok = RAND_status() == 1;
		}
	} else if (argv[1][0] == '6') {
		puts("BEFORE_OPENSSL_INIT_CRYPTO");
		ok = crypto_init();
		if (ok) {
			puts("BEFORE_RAND_POLL");
			ok = RAND_poll() == 1;
		}
		if (ok) {
			puts("BEFORE_RAND_STATUS");
			ok = RAND_status() == 1;
		}
	} else if (argv[1][0] == '8') {
		unsigned char seed[32];
		unsigned char bytes[32];
		int i;
		for (i = 0; i < (int)sizeof(seed); i++) seed[i] = (unsigned char)(i * 13 + 7);
		puts("BEFORE_OPENSSL_INIT_CRYPTO");
		ok = crypto_init();
		if (ok) {
			puts("BEFORE_TEST_ONLY_RAND_ADD");
			RAND_add(seed, (int)sizeof(seed), (double)sizeof(seed));
			puts("BEFORE_RAND_STATUS");
			ok = RAND_status() == 1;
		}
		if (ok) {
			puts("BEFORE_RAND_BYTES");
			ok = RAND_bytes(bytes, (int)sizeof(bytes)) == 1;
		}
	} else if (argv[1][0] == '5') {
		puts("BEFORE_OPENSSL_INIT_CRYPTO");
		ok = crypto_init();
		if (ok) {
			puts("OPENSSL_INIT_CRYPTO_PASS");
			puts("BEFORE_PROVIDER_LOAD");
			ok = OSSL_PROVIDER_load(NULL, "default") != NULL;
		}
	} else {
		return 64;
	}
	puts(ok ? "OPENSSL_STEP_PASS" : "OPENSSL_STEP_FAIL");
	return ok ? 0 : 1;
}
