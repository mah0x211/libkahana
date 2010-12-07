/*
 *  kahana_cipher.h
 *
 *  Created by Mah on 10/04/20
 *  Copyright 2010 Masatoshi Teruya All rights reserved.
 *
 */
#ifndef ___KAHANA_CIPHER___
#define ___KAHANA_CIPHER___

#include "kahana.h"
#include "mcrypt.h"

typedef struct kCipher_t kCipher_t;

kCipher_t *kahanaCipherInit( apr_pool_t *p, const char *cipher, const char *mode, const char *iv, const char **errstr );
void kahanaCipherDestroy( kCipher_t *co );

const char *kahanaCipherEncData( kCipher_t *co );
// [base64url]@[initialize_vector]
const char *kahanaCipherEncDataBase64( kCipher_t *co );
size_t kahanaCipherEncBytes( kCipher_t *co );

const char *kahanaCipherDecData( kCipher_t *co );
size_t kahanaCipherDecBytes( kCipher_t *co );

const char *kahanaCipherIVFromEncData( const char *base64url_str );
void kahanaCipherSetIV( kCipher_t *co, const char *iv );

// str = string, clue = secret key
const char *kahanaCipherEncrypt( kCipher_t *co, const char *str, const char *clue );
// str = base64url string, clue secret key
const char *kahanaCipherDecrypt( kCipher_t *co, const char *base64, const char *clue );


#endif
