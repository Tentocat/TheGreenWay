
#ifndef TWEETNACL_H
#define TWEETNACL_H
#define crypto_auth_PRIMITIVE "hmacsha512256"
#define crypto_auth crypto_auth_hmacsha512256
#define crypto_auth_verify crypto_auth_hmacsha512256_verify
#define crypto_auth_BYTES crypto_auth_hmacsha512256_BYTES
#define crypto_auth_KEYBYTES crypto_auth_hmacsha512256_KEYBYTES
#define crypto_auth_IMPLEMENTATION crypto_auth_hmacsha512256_IMPLEMENTATION
#define crypto_auth_VERSION crypto_auth_hmacsha512256_VERSION
#define crypto_auth_hmacsha512256_tweet_BYTES 32
#define crypto_auth_hmacsha512256_tweet_KEYBYTES 32
extern int crypto_auth_hmacsha512256_tweet(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *);
extern int crypto_auth_hmacsha512256_tweet_verify(const unsigned char *,const unsigned char *,unsigned long long,const unsigned char *);
#define crypto_auth_hmacsha512256_tweet_VERSION "-"
#define crypto_auth_hmacsha512256 crypto_auth_hmacsha512256_tweet
#define crypto_auth_hmacsha512256_verify crypto_auth_hmacsha512256_tweet_verify
#define crypto_auth_hmacsha512256_BYTES crypto_auth_hmacsha512256_tweet_BYTES
#define crypto_auth_hmacsha512256_KEYBYTES crypto_auth_hmacsha512256_tweet_KEYBYTES
#define crypto_auth_hmacsha512256_VERSION crypto_auth_hmacsha512256_tweet_VERSION
#define crypto_auth_hmacsha512256_IMPLEMENTATION "crypto_auth/hmacsha512256/tweet"
#define crypto_box_PRIMITIVE "curve25519xsalsa20poly1305"
#define crypto_box crypto_box_curve25519xsalsa20poly1305
#define crypto_box_open crypto_box_curve25519xsalsa20poly1305_open
#define crypto_box_keypair crypto_box_curve25519xsalsa20poly1305_keypair
#define crypto_box_beforenm crypto_box_curve25519xsalsa20poly1305_beforenm
#define crypto_box_afternm crypto_box_curve25519xsalsa20poly1305_afternm
#define crypto_box_open_afternm crypto_box_curve25519xsalsa20poly1305_open_afternm
#define crypto_box_PUBLICKEYBYTES crypto_box_curve25519xsalsa20poly1305_PUBLICKEYBYTES
#define crypto_box_SECRETKEYBYTES crypto_box_curve25519xsalsa20poly1305_SECRETKEYBYTES
#define crypto_box_BEFORENMBYTES crypto_box_curve25519xsalsa20poly1305_BEFORENMBYTES
#define crypto_box_NONCEBYTES crypto_box_curve25519xsalsa20poly1305_NONCEBYTES
#define crypto_box_ZEROBYTES crypto_box_curve25519xsalsa20poly1305_ZEROBYTES
#define crypto_box_BOXZEROBYTES crypto_box_curve25519xsalsa20poly1305_BOXZEROBYTES
#define crypto_box_IMPLEMENTATION crypto_box_curve25519xsalsa20poly1305_IMPLEMENTATION
#define crypto_box_VERSION crypto_box_curve25519xsalsa20poly1305_VERSION
#define crypto_box_curve25519xsalsa20poly1305_tweet_PUBLICKEYBYTES 32
#define crypto_box_curve25519xsalsa20poly1305_tweet_SECRETKEYBYTES 32
#define crypto_box_curve25519xsalsa20poly1305_tweet_BEFORENMBYTES 32
#define crypto_box_curve25519xsalsa20poly1305_tweet_NONCEBYTES 24
#define crypto_box_curve25519xsalsa20poly1305_tweet_ZEROBYTES 32
#define crypto_box_curve25519xsalsa20poly1305_tweet_BOXZEROBYTES 16
extern int crypto_box_curve25519xsalsa20poly1305_tweet(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *,const unsigned char *);
extern int crypto_box_curve25519xsalsa20poly1305_tweet_open(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *,const unsigned char *);
extern int crypto_box_curve25519xsalsa20poly1305_tweet_keypair(unsigned char *,unsigned char *);
extern int crypto_box_curve25519xsalsa20poly1305_tweet_beforenm(unsigned char *,const unsigned char *,const unsigned char *);
extern int crypto_box_curve25519xsalsa20poly1305_tweet_afternm(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
extern int crypto_box_curve25519xsalsa20poly1305_tweet_open_afternm(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
#define crypto_box_curve25519xsalsa20poly1305_tweet_VERSION "-"
#define crypto_box_curve25519xsalsa20poly1305 crypto_box_curve25519xsalsa20poly1305_tweet
#define crypto_box_curve25519xsalsa20poly1305_open crypto_box_curve25519xsalsa20poly1305_tweet_open
#define crypto_box_curve25519xsalsa20poly1305_keypair crypto_box_curve25519xsalsa20poly1305_tweet_keypair
#define crypto_box_curve25519xsalsa20poly1305_beforenm crypto_box_curve25519xsalsa20poly1305_tweet_beforenm
#define crypto_box_curve25519xsalsa20poly1305_afternm crypto_box_curve25519xsalsa20poly1305_tweet_afternm
#define crypto_box_curve25519xsalsa20poly1305_open_afternm crypto_box_curve25519xsalsa20poly1305_tweet_open_afternm
#define crypto_box_curve25519xsalsa20poly1305_PUBLICKEYBYTES crypto_box_curve25519xsalsa20poly1305_tweet_PUBLICKEYBYTES
#define crypto_box_curve25519xsalsa20poly1305_SECRETKEYBYTES crypto_box_curve25519xsalsa20poly1305_tweet_SECRETKEYBYTES
#define crypto_box_curve25519xsalsa20poly1305_BEFORENMBYTES crypto_box_curve25519xsalsa20poly1305_tweet_BEFORENMBYTES
#define crypto_box_curve25519xsalsa20poly1305_NONCEBYTES crypto_box_curve25519xsalsa20poly1305_tweet_NONCEBYTES
#define crypto_box_curve25519xsalsa20poly1305_ZEROBYTES crypto_box_curve25519xsalsa20poly1305_tweet_ZEROBYTES
#define crypto_box_curve25519xsalsa20poly1305_BOXZEROBYTES crypto_box_curve25519xsalsa20poly1305_tweet_BOXZEROBYTES
#define crypto_box_curve25519xsalsa20poly1305_VERSION crypto_box_curve25519xsalsa20poly1305_tweet_VERSION
#define crypto_box_curve25519xsalsa20poly1305_IMPLEMENTATION "crypto_box/curve25519xsalsa20poly1305/tweet"
#define crypto_core_PRIMITIVE "salsa20"
#define crypto_core crypto_core_salsa20
#define crypto_core_OUTPUTBYTES crypto_core_salsa20_OUTPUTBYTES
#define crypto_core_INPUTBYTES crypto_core_salsa20_INPUTBYTES
#define crypto_core_KEYBYTES crypto_core_salsa20_KEYBYTES
#define crypto_core_CONSTBYTES crypto_core_salsa20_CONSTBYTES
#define crypto_core_IMPLEMENTATION crypto_core_salsa20_IMPLEMENTATION
#define crypto_core_VERSION crypto_core_salsa20_VERSION
#define crypto_core_salsa20_tweet_OUTPUTBYTES 64
#define crypto_core_salsa20_tweet_INPUTBYTES 16
#define crypto_core_salsa20_tweet_KEYBYTES 32
#define crypto_core_salsa20_tweet_CONSTBYTES 16
extern int crypto_core_salsa20_tweet(unsigned char *,const unsigned char *,const unsigned char *,const unsigned char *);
#define crypto_core_salsa20_tweet_VERSION "-"
#define crypto_core_salsa20 crypto_core_salsa20_tweet
#define crypto_core_salsa20_OUTPUTBYTES crypto_core_salsa20_tweet_OUTPUTBYTES
#define crypto_core_salsa20_INPUTBYTES crypto_core_salsa20_tweet_INPUTBYTES
#define crypto_core_salsa20_KEYBYTES crypto_core_salsa20_tweet_KEYBYTES
#define crypto_core_salsa20_CONSTBYTES crypto_core_salsa20_tweet_CONSTBYTES
#define crypto_core_salsa20_VERSION crypto_core_salsa20_tweet_VERSION
#define crypto_core_salsa20_IMPLEMENTATION "crypto_core/salsa20/tweet"
#define crypto_core_hsalsa20_tweet_OUTPUTBYTES 32
#define crypto_core_hsalsa20_tweet_INPUTBYTES 16
#define crypto_core_hsalsa20_tweet_KEYBYTES 32
#define crypto_core_hsalsa20_tweet_CONSTBYTES 16
extern int crypto_core_hsalsa20_tweet(unsigned char *,const unsigned char *,const unsigned char *,const unsigned char *);
#define crypto_core_hsalsa20_tweet_VERSION "-"
#define crypto_core_hsalsa20 crypto_core_hsalsa20_tweet
#define crypto_core_hsalsa20_OUTPUTBYTES crypto_core_hsalsa20_tweet_OUTPUTBYTES
#define crypto_core_hsalsa20_INPUTBYTES crypto_core_hsalsa20_tweet_INPUTBYTES
#define crypto_core_hsalsa20_KEYBYTES crypto_core_hsalsa20_tweet_KEYBYTES
#define crypto_core_hsalsa20_CONSTBYTES crypto_core_hsalsa20_tweet_CONSTBYTES
#define crypto_core_hsalsa20_VERSION crypto_core_hsalsa20_tweet_VERSION
#define crypto_core_hsalsa20_IMPLEMENTATION "crypto_core/hsalsa20/tweet"
#define crypto_hashblocks_PRIMITIVE "sha512"
#define crypto_hashblocks crypto_hashblocks_sha512
#define crypto_hashblocks_STATEBYTES crypto_hashblocks_sha512_STATEBYTES
#define crypto_hashblocks_BLOCKBYTES crypto_hashblocks_sha512_BLOCKBYTES
#define crypto_hashblocks_IMPLEMENTATION crypto_hashblocks_sha512_IMPLEMENTATION
#define crypto_hashblocks_VERSION crypto_hashblocks_sha512_VERSION
#define crypto_hashblocks_sha512_tweet_STATEBYTES 64
#define crypto_hashblocks_sha512_tweet_BLOCKBYTES 128
extern int crypto_hashblocks_sha512_tweet(unsigned char *,const unsigned char *,unsigned long long);
#define crypto_hashblocks_sha512_tweet_VERSION "-"
#define crypto_hashblocks_sha512 crypto_hashblocks_sha512_tweet
#define crypto_hashblocks_sha512_STATEBYTES crypto_hashblocks_sha512_tweet_STATEBYTES
#define crypto_hashblocks_sha512_BLOCKBYTES crypto_hashblocks_sha512_tweet_BLOCKBYTES
#define crypto_hashblocks_sha512_VERSION crypto_hashblocks_sha512_tweet_VERSION
#define crypto_hashblocks_sha512_IMPLEMENTATION "crypto_hashblocks/sha512/tweet"
#define crypto_hashblocks_sha256_tweet_STATEBYTES 32
#define crypto_hashblocks_sha256_tweet_BLOCKBYTES 64
extern int crypto_hashblocks_sha256_tweet(unsigned char *,const unsigned char *,unsigned long long);
#define crypto_hashblocks_sha256_tweet_VERSION "-"
#define crypto_hashblocks_sha256 crypto_hashblocks_sha256_tweet
#define crypto_hashblocks_sha256_STATEBYTES crypto_hashblocks_sha256_tweet_STATEBYTES
#define crypto_hashblocks_sha256_BLOCKBYTES crypto_hashblocks_sha256_tweet_BLOCKBYTES
#define crypto_hashblocks_sha256_VERSION crypto_hashblocks_sha256_tweet_VERSION
#define crypto_hashblocks_sha256_IMPLEMENTATION "crypto_hashblocks/sha256/tweet"
#define crypto_hash_PRIMITIVE "sha512"
#define crypto_hash crypto_hash_sha512
#define crypto_hash_BYTES crypto_hash_sha512_BYTES
#define crypto_hash_IMPLEMENTATION crypto_hash_sha512_IMPLEMENTATION
#define crypto_hash_VERSION crypto_hash_sha512_VERSION
#define crypto_hash_sha512_tweet_BYTES 64
extern int crypto_hash_sha512_tweet(unsigned char *,const unsigned char *,unsigned long long);
#define crypto_hash_sha512_tweet_VERSION "-"
#define crypto_hash_sha512 crypto_hash_sha512_tweet
#define crypto_hash_sha512_BYTES crypto_hash_sha512_tweet_BYTES
#define crypto_hash_sha512_VERSION crypto_hash_sha512_tweet_VERSION
#define crypto_hash_sha512_IMPLEMENTATION "crypto_hash/sha512/tweet"
#define crypto_hash_sha256_tweet_BYTES 32
extern int crypto_hash_sha256_tweet(unsigned char *,const unsigned char *,unsigned long long);
#define crypto_hash_sha256_tweet_VERSION "-"
#define crypto_hash_sha256 crypto_hash_sha256_tweet
#define crypto_hash_sha256_BYTES crypto_hash_sha256_tweet_BYTES
#define crypto_hash_sha256_VERSION crypto_hash_sha256_tweet_VERSION
#define crypto_hash_sha256_IMPLEMENTATION "crypto_hash/sha256/tweet"
#define crypto_onetimeauth_PRIMITIVE "poly1305"
#define crypto_onetimeauth crypto_onetimeauth_poly1305
#define crypto_onetimeauth_verify crypto_onetimeauth_poly1305_verify
#define crypto_onetimeauth_BYTES crypto_onetimeauth_poly1305_BYTES
#define crypto_onetimeauth_KEYBYTES crypto_onetimeauth_poly1305_KEYBYTES
#define crypto_onetimeauth_IMPLEMENTATION crypto_onetimeauth_poly1305_IMPLEMENTATION
#define crypto_onetimeauth_VERSION crypto_onetimeauth_poly1305_VERSION
#define crypto_onetimeauth_poly1305_tweet_BYTES 16
#define crypto_onetimeauth_poly1305_tweet_KEYBYTES 32
extern int crypto_onetimeauth_poly1305_tweet(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *);
extern int crypto_onetimeauth_poly1305_tweet_verify(const unsigned char *,const unsigned char *,unsigned long long,const unsigned char *);
#define crypto_onetimeauth_poly1305_tweet_VERSION "-"
#define crypto_onetimeauth_poly1305 crypto_onetimeauth_poly1305_tweet
#define crypto_onetimeauth_poly1305_verify crypto_onetimeauth_poly1305_tweet_verify
#define crypto_onetimeauth_poly1305_BYTES crypto_onetimeauth_poly1305_tweet_BYTES
#define crypto_onetimeauth_poly1305_KEYBYTES crypto_onetimeauth_poly1305_tweet_KEYBYTES
#define crypto_onetimeauth_poly1305_VERSION crypto_onetimeauth_poly1305_tweet_VERSION
#define crypto_onetimeauth_poly1305_IMPLEMENTATION "crypto_onetimeauth/poly1305/tweet"
#define crypto_scalarmult_PRIMITIVE "curve25519"
#define crypto_scalarmult crypto_scalarmult_curve25519
#define crypto_scalarmult_base crypto_scalarmult_curve25519_base
#define crypto_scalarmult_BYTES crypto_scalarmult_curve25519_BYTES
#define crypto_scalarmult_SCALARBYTES crypto_scalarmult_curve25519_SCALARBYTES
#define crypto_scalarmult_IMPLEMENTATION crypto_scalarmult_curve25519_IMPLEMENTATION
#define crypto_scalarmult_VERSION crypto_scalarmult_curve25519_VERSION
#define crypto_scalarmult_curve25519_tweet_BYTES 32
#define crypto_scalarmult_curve25519_tweet_SCALARBYTES 32
extern int crypto_scalarmult_curve25519_tweet(unsigned char *,const unsigned char *,const unsigned char *);
extern int crypto_scalarmult_curve25519_tweet_base(unsigned char *,const unsigned char *);
#define crypto_scalarmult_curve25519_tweet_VERSION "-"
#define crypto_scalarmult_curve25519 crypto_scalarmult_curve25519_tweet
#define crypto_scalarmult_curve25519_base crypto_scalarmult_curve25519_tweet_base
#define crypto_scalarmult_curve25519_BYTES crypto_scalarmult_curve25519_tweet_BYTES
#define crypto_scalarmult_curve25519_SCALARBYTES crypto_scalarmult_curve25519_tweet_SCALARBYTES
#define crypto_scalarmult_curve25519_VERSION crypto_scalarmult_curve25519_tweet_VERSION
#define crypto_scalarmult_curve25519_IMPLEMENTATION "crypto_scalarmult/curve25519/tweet"
#define crypto_secretbox_PRIMITIVE "xsalsa20poly1305"
#define crypto_secretbox crypto_secretbox_xsalsa20poly1305
#define crypto_secretbox_open crypto_secretbox_xsalsa20poly1305_open
#define crypto_secretbox_KEYBYTES crypto_secretbox_xsalsa20poly1305_KEYBYTES
#define crypto_secretbox_NONCEBYTES crypto_secretbox_xsalsa20poly1305_NONCEBYTES
#define crypto_secretbox_ZEROBYTES crypto_secretbox_xsalsa20poly1305_ZEROBYTES
#define crypto_secretbox_BOXZEROBYTES crypto_secretbox_xsalsa20poly1305_BOXZEROBYTES
#define crypto_secretbox_IMPLEMENTATION crypto_secretbox_xsalsa20poly1305_IMPLEMENTATION
#define crypto_secretbox_VERSION crypto_secretbox_xsalsa20poly1305_VERSION
#define crypto_secretbox_xsalsa20poly1305_tweet_KEYBYTES 32
#define crypto_secretbox_xsalsa20poly1305_tweet_NONCEBYTES 24
#define crypto_secretbox_xsalsa20poly1305_tweet_ZEROBYTES 32
#define crypto_secretbox_xsalsa20poly1305_tweet_BOXZEROBYTES 16
extern int crypto_secretbox_xsalsa20poly1305_tweet(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
extern int crypto_secretbox_xsalsa20poly1305_tweet_open(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
#define crypto_secretbox_xsalsa20poly1305_tweet_VERSION "-"
#define crypto_secretbox_xsalsa20poly1305 crypto_secretbox_xsalsa20poly1305_tweet
#define crypto_secretbox_xsalsa20poly1305_open crypto_secretbox_xsalsa20poly1305_tweet_open
#define crypto_secretbox_xsalsa20poly1305_KEYBYTES crypto_secretbox_xsalsa20poly1305_tweet_KEYBYTES
#define crypto_secretbox_xsalsa20poly1305_NONCEBYTES crypto_secretbox_xsalsa20poly1305_tweet_NONCEBYTES
#define crypto_secretbox_xsalsa20poly1305_ZEROBYTES crypto_secretbox_xsalsa20poly1305_tweet_ZEROBYTES
#define crypto_secretbox_xsalsa20poly1305_BOXZEROBYTES crypto_secretbox_xsalsa20poly1305_tweet_BOXZEROBYTES
#define crypto_secretbox_xsalsa20poly1305_VERSION crypto_secretbox_xsalsa20poly1305_tweet_VERSION
#define crypto_secretbox_xsalsa20poly1305_IMPLEMENTATION "crypto_secretbox/xsalsa20poly1305/tweet"
#define crypto_sign_PRIMITIVE "ed25519"
#define crypto_sign crypto_sign_ed25519
#define crypto_sign_open crypto_sign_ed25519_open
#define crypto_sign_keypair crypto_sign_ed25519_keypair
#define crypto_sign_BYTES crypto_sign_ed25519_BYTES
#define crypto_sign_PUBLICKEYBYTES crypto_sign_ed25519_PUBLICKEYBYTES
#define crypto_sign_SECRETKEYBYTES crypto_sign_ed25519_SECRETKEYBYTES
#define crypto_sign_IMPLEMENTATION crypto_sign_ed25519_IMPLEMENTATION
#define crypto_sign_VERSION crypto_sign_ed25519_VERSION
#define crypto_sign_ed25519_tweet_BYTES 64
#define crypto_sign_ed25519_tweet_PUBLICKEYBYTES 32
#define crypto_sign_ed25519_tweet_SECRETKEYBYTES 64
extern int crypto_sign_ed25519_tweet(unsigned char *,unsigned long long *,const unsigned char *,unsigned long long,const unsigned char *);
extern int crypto_sign_ed25519_tweet_open(unsigned char *,unsigned long long *,const unsigned char *,unsigned long long,const unsigned char *);
extern int crypto_sign_ed25519_tweet_keypair(unsigned char *,unsigned char *);
#define crypto_sign_ed25519_tweet_VERSION "-"
#define crypto_sign_ed25519 crypto_sign_ed25519_tweet
#define crypto_sign_ed25519_open crypto_sign_ed25519_tweet_open
#define crypto_sign_ed25519_keypair crypto_sign_ed25519_tweet_keypair
#define crypto_sign_ed25519_BYTES crypto_sign_ed25519_tweet_BYTES
#define crypto_sign_ed25519_PUBLICKEYBYTES crypto_sign_ed25519_tweet_PUBLICKEYBYTES
#define crypto_sign_ed25519_SECRETKEYBYTES crypto_sign_ed25519_tweet_SECRETKEYBYTES
#define crypto_sign_ed25519_VERSION crypto_sign_ed25519_tweet_VERSION
#define crypto_sign_ed25519_IMPLEMENTATION "crypto_sign/ed25519/tweet"
#define crypto_stream_PRIMITIVE "xsalsa20"
#define crypto_stream crypto_stream_xsalsa20
#define crypto_stream_xor crypto_stream_xsalsa20_xor
#define crypto_stream_KEYBYTES crypto_stream_xsalsa20_KEYBYTES
#define crypto_stream_NONCEBYTES crypto_stream_xsalsa20_NONCEBYTES
#define crypto_stream_IMPLEMENTATION crypto_stream_xsalsa20_IMPLEMENTATION
#define crypto_stream_VERSION crypto_stream_xsalsa20_VERSION
#define crypto_stream_xsalsa20_tweet_KEYBYTES 32
#define crypto_stream_xsalsa20_tweet_NONCEBYTES 24
extern int crypto_stream_xsalsa20_tweet(unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
extern int crypto_stream_xsalsa20_tweet_xor(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
#define crypto_stream_xsalsa20_tweet_VERSION "-"
#define crypto_stream_xsalsa20 crypto_stream_xsalsa20_tweet
#define crypto_stream_xsalsa20_xor crypto_stream_xsalsa20_tweet_xor
#define crypto_stream_xsalsa20_KEYBYTES crypto_stream_xsalsa20_tweet_KEYBYTES
#define crypto_stream_xsalsa20_NONCEBYTES crypto_stream_xsalsa20_tweet_NONCEBYTES
#define crypto_stream_xsalsa20_VERSION crypto_stream_xsalsa20_tweet_VERSION
#define crypto_stream_xsalsa20_IMPLEMENTATION "crypto_stream/xsalsa20/tweet"
#define crypto_stream_salsa20_tweet_KEYBYTES 32
#define crypto_stream_salsa20_tweet_NONCEBYTES 8
extern int crypto_stream_salsa20_tweet(unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
extern int crypto_stream_salsa20_tweet_xor(unsigned char *,const unsigned char *,unsigned long long,const unsigned char *,const unsigned char *);
#define crypto_stream_salsa20_tweet_VERSION "-"
#define crypto_stream_salsa20 crypto_stream_salsa20_tweet
#define crypto_stream_salsa20_xor crypto_stream_salsa20_tweet_xor
#define crypto_stream_salsa20_KEYBYTES crypto_stream_salsa20_tweet_KEYBYTES
#define crypto_stream_salsa20_NONCEBYTES crypto_stream_salsa20_tweet_NONCEBYTES
#define crypto_stream_salsa20_VERSION crypto_stream_salsa20_tweet_VERSION
#define crypto_stream_salsa20_IMPLEMENTATION "crypto_stream/salsa20/tweet"
#define crypto_verify_PRIMITIVE "16"
#define crypto_verify crypto_verify_16
#define crypto_verify_BYTES crypto_verify_16_BYTES
#define crypto_verify_IMPLEMENTATION crypto_verify_16_IMPLEMENTATION
#define crypto_verify_VERSION crypto_verify_16_VERSION
#define crypto_verify_16_tweet_BYTES 16
extern int crypto_verify_16_tweet(const unsigned char *,const unsigned char *);
#define crypto_verify_16_tweet_VERSION "-"
#define crypto_verify_16 crypto_verify_16_tweet
#define crypto_verify_16_BYTES crypto_verify_16_tweet_BYTES
#define crypto_verify_16_VERSION crypto_verify_16_tweet_VERSION
#define crypto_verify_16_IMPLEMENTATION "crypto_verify/16/tweet"
#define crypto_verify_32_tweet_BYTES 32
extern int crypto_verify_32_tweet(const unsigned char *,const unsigned char *);
#define crypto_verify_32_tweet_VERSION "-"
#define crypto_verify_32 crypto_verify_32_tweet
#define crypto_verify_32_BYTES crypto_verify_32_tweet_BYTES
#define crypto_verify_32_VERSION crypto_verify_32_tweet_VERSION
#define crypto_verify_32_IMPLEMENTATION "crypto_verify/32/tweet"
#endif