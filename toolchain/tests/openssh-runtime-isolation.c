#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <zlib.h>

static void mark(const char *name, int ok)
{
	printf("%s=%s\n", name, ok ? "PASS" : "FAIL");	fflush(stdout);
}

int main(void)
{
	char *p = malloc(64);
	int heap_ok = p != NULL;
	if (heap_ok) {
		memset(p, 'a', 63); p[63] = 0;
		heap_ok = strlen(p) == 63 && memcmp(p, "aaaaaaaa", 8) == 0;
		free(p);
	}
	mark("HEAP_STRING", heap_ok);
	if (!heap_ok) return 1;

	z_stream zs = {0};
	unsigned char compressed[128], plain[16];
	const unsigned char input[] = "REM transfer probe";
	int zret = deflateInit(&zs, Z_BEST_SPEED);
	if (zret == Z_OK) {
		zs.next_in = (Bytef *)input; zs.avail_in = sizeof(input);
		zs.next_out = compressed; zs.avail_out = sizeof(compressed);
		zret = deflate(&zs, Z_FINISH);
		int compressed_ok = zret == Z_STREAM_END;
		deflateEnd(&zs);
		mark("ZLIB_COMPRESS", compressed_ok);
		if (!compressed_ok) return 3;
	} else { mark("ZLIB_COMPRESS", 0); return 3; }
	if (inflateInit(&zs) != Z_OK) { mark("ZLIB_INFLATE", 0); return 4; }
	zs.next_in = compressed; zs.avail_in = sizeof(compressed);
	zs.next_out = plain; zs.avail_out = sizeof(plain);
	zret = inflate(&zs, Z_FINISH);
	int inflate_ok = zret == Z_STREAM_END && zs.total_out == sizeof(input) &&
		memcmp(plain, input, sizeof(input)) == 0;
	inflateEnd(&zs);
	mark("ZLIB_INFLATE", inflate_ok);
	if (!inflate_ok) return 4;

	mark("OPENSSL_VERSION", OpenSSL_version_num() != 0);
	if (OPENSSL_init_crypto(OPENSSL_INIT_NO_LOAD_CONFIG, NULL) != 1) {
		mark("OPENSSL_INIT", 0); return 2;
	}
	mark("OPENSSL_INIT", 1);

	unsigned char digest[SHA256_DIGEST_LENGTH];
	static const unsigned char expected[SHA256_DIGEST_LENGTH] = {
		0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,
		0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
		0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,
		0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad
	};
	mark("CRYPTO_SHA256", SHA256((const unsigned char *)"abc", 3, digest) == digest &&
	     memcmp(digest, expected, sizeof(expected)) == 0);

	EVP_MD_CTX *ctx = EVP_MD_CTX_new();
	unsigned int digest_len = 0;
	int evp_ok = ctx && EVP_DigestInit_ex(ctx, EVP_sha256(), NULL) == 1 &&
		EVP_DigestUpdate(ctx, "abc", 3) == 1 &&
		EVP_DigestFinal_ex(ctx, digest, &digest_len) == 1 &&
		digest_len == SHA256_DIGEST_LENGTH &&
		memcmp(digest, expected, sizeof(expected)) == 0;
	EVP_MD_CTX_free(ctx);
	mark("EVP_SHA256", evp_ok);
	return evp_ok ? 0 : 5;
}
