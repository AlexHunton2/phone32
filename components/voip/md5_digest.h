/* md5_digest.h */

#ifndef MD5_DIGEST_H
#define MD5_DIGEST_H

#define MD5_DIGEST_LENGTH 16

// dest, src, src_len
int get_digest(
    char *,       // dest
    char *,       // auth_ptr
    const char *, // username
    const char *, // password
    char *,       // digest_uri
    char *,       // realm
    char *,       // nonce
    char *        // method
    );

#endif
