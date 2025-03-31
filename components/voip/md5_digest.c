/* md5_digest.c */

#include "mbedtls/md5.h"
#include <string.h>

#include "md5_digest.h"

// NOTE: dest MUST be at least of size MD5_DIGEST_LENGTH*2 (to store hex encoding)
static int get_md5_string(char * dest, uint8_t * src, size_t src_len) {

  uint8_t md[MD5_DIGEST_LENGTH];

  // get the hash
  mbedtls_md5_context ctx;
  mbedtls_md5_init(&ctx);
  if (mbedtls_md5_starts(&ctx)) {
    return 1;
  }
  if (mbedtls_md5_update(&ctx, src, src_len)) {
    return 1;
  }
  if (mbedtls_md5_finish(&ctx, md)) {
    return 1;
  }
  mbedtls_md5_free(&ctx);

  // output the hash in string form to dest (hex encoded)
  int i;
  for (i = 0; i < MD5_DIGEST_LENGTH; i++) {
    uint8_t high_nibble = ((md[i] >> 4) & 0xF);
    uint8_t low_nibble = (md[i] & 0xF);
    if (high_nibble < 10) {
      dest[i << 1] = high_nibble + 48;
    }
    else {
      dest[i << 1] = high_nibble + 87;
    }
    if (low_nibble < 10) {
      dest[(i << 1) + 1] = low_nibble + 48;
    }
    else {
      dest[(i << 1) + 1] = low_nibble + 87;
    }
  }
  return 0;
}

int get_digest(char * dest,
    char * auth_ptr,
    const char * username,
    const char * password,
    char * digest_uri,
    char * realm,
    char * nonce,
    char * method) {


  *strchr(realm, '"') = '\0';
  *strchr(nonce, '"') = '\0';

  char hash_input_1[strlen(username) + strlen(realm) + strlen(password) + 2];
  char hash_input_2[strlen(method) + strlen(digest_uri) + 1];
  char final_hash_input[4 * MD5_DIGEST_LENGTH + strlen(nonce) + 2];

  int i = 0;
  strcpy(hash_input_1, username);
  hash_input_1[i += strlen(username)] = ':';
  strcpy(&hash_input_1[++i], realm);
  hash_input_1[i += strlen(realm)] = ':';
  strcpy(&hash_input_1[++i], password);
  get_md5_string(final_hash_input, (uint8_t *)hash_input_1, sizeof(hash_input_1));

  int h = 2 * MD5_DIGEST_LENGTH;
  final_hash_input[h] = ':';
  strcpy(&final_hash_input[++h], nonce);
  final_hash_input[h += strlen(nonce)] = ':';

  i = 0;
  strcpy(hash_input_2, method);
  hash_input_2[i += strlen(method)] = ':';
  strcpy(&hash_input_2[++i], digest_uri);
  get_md5_string(&final_hash_input[h + 1], (uint8_t *)hash_input_2, sizeof(hash_input_2));
  get_md5_string(dest, (uint8_t *)final_hash_input, sizeof(final_hash_input));
  return 0;
}
