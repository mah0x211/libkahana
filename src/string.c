/* 
**
**  Modify : 4/13/2010
**  (C) Masatoshi Teruya
**
*/
#include "kahana_string.h"
#include "kahana_private.h"

/*
	Dependency
		apr_pool_t
		apr_hash_t
		apr_table_t
		apr_array_header_t
		apr_psprintf
		apr_pstrndup
		apr_pstrdup
		apr_pstrcat
		apr_strtok
*/

struct kJapaneseCharactorMap_t {
	apr_pool_t *p;
	// full-width <-> half-width
	apr_hash_t *fh;
	// hiragana <-> katakana
	apr_hash_t *hk;
	// half-width
	kRegexp_t *h2fSym;
	kRegexp_t *h2fNum;
	kRegexp_t *h2fAlpha;
	kRegexp_t *h2fAlphaLC;
	kRegexp_t *h2fAlphaUC;
	kRegexp_t *h2fKana;
	kRegexp_t *h2fKanaD;
	// full-width
	kRegexp_t *f2hSym;
	kRegexp_t *f2hNum;
	kRegexp_t *f2hAlpha;
	kRegexp_t *f2hAlphaLC;
	kRegexp_t *f2hAlphaUC;
	kRegexp_t *f2hKana;
	kRegexp_t *f2hKanaD;
};


// MARK:  Japanese CodeMap
/*
	Japanese hiragana half-width <-> full-width CharactorMap
	
	Regex: 全角半角カナ

	半角数字 = [0-9]
	全角数字 = [\xef\xbc\x90-\xef\xbc\x99]
	
	半角小英字 = [a-z]
	全角小英字 = [\xef\xbc\xa1-\xef\xbc\xba]
	
	半角大英字 = [A-Z]
	全角大英字 = [\xef\xbd\x81-\xef\xbd\x9a]
	
	半角カナ濁点 = (?:[\xef\xbd\xb3\xef\xbd\xb6-\xef\xbd\xbf\xef\xbe\x80-\xef\xbe\x84\xef\xbe\x8a-\xef\xbe\x8e]\xef\xbe\x9e|[\xef\xbe\x8a-\xef\xbe\x8e]\xef\xbe\x9f)
	全角カナ濁点 = [\xe3\x83\xb4\xe3\x82\xac\xe3\x82\xae\xe3\x82\xb0\xe3\x82\xb2\xe3\x82\xb4\xe3\x82\xb6\xe3\x82\xb8\xe3\x82\xba\xe3\x82\xbc\xe3\x82\xbe\xe3\x83\x80\xe3\x83\x82\xe3\x83\x85\xe3\x83\x87\xe3\x83\x89\xe3\x83\x90\xe3\x83\x91\xe3\x83\x93\xe3\x83\x94\xe3\x83\x96\xe3\x83\x97\xe3\x83\x99\xe3\x83\x9a\xe3\x83\x9c\xe3\x83\x9d]
	
	半角カナ = [\xef\xbd\xa1-\xef\xbd\xbf\xef\xbe\x80-\xef\xbe\x9f]
	全角カナ = [\xe3\x80\x81\xe3\x80\x82\xe3\x80\x8c\xe3\x80\x8d\xe3\x82\x9b\xe3\x82\x9c\xe3\x82\xa1-\xe3\x82\xaf\xe3\x82\xb1\xe3\x82\xb3\xe3\x82\xb5\xe3\x82\xb7\xe3\x82\xb9\xe3\x82\xbb\xe3\x82\xbd\xe3\x82\xbf\xe3\x83\x81\xe3\x83\x83\xe3\x83\x84\xe3\x83\x86\xe3\x83\x88\xe3\x83\x8a-\xe3\x83\x8f\xe3\x83\x92\xe3\x83\x95\xe3\x83\x98\xe3\x83\x9b\xe3\x83\x9e\xe3\x83\x9f\xe3\x83\xa0-\xe3\x83\xad\xe3\x83\xaf\xe3\x83\xb2\xe3\x83\xb3\xe3\x83\xbb\xe3\x83\xbc]
	
	半角記号 = [_\x20-\x2f\x3a-\x40\x5b-\x5e\x60\x7b-\x7e]
	全角記号 = [\xe2\x80\x99\xe2\x80\x9d\xe3\x80\x80\xe3\x80\x9c\xef\xbc\x81\xef\xbc\x83-\xef\xbc\x86\xef\xbc\x88-\xef\xbc\x8f\xef\xbc\x9a-\xef\xbc\xa0\xef\xbc\xbb\xef\xbc\xbd-\xef\xbc\xbf\xef\xbd\x80\xef\xbd\x9b-\xef\xbd\x9e\xef\xbf\xa5]

*/

static kJapaneseCharactorMap_t *GetJaCharMap( void )
{
	Kahana_t *kahana = _kahanaGlobal();
	return ( kahana ) ? kahana->jaMap : NULL;
}

// callback
static inline const char *cbJaCharMap( size_t match, const char **group, void *ctx ){
	return kahanaStrJaMapFH( group[0] );
}

static apr_status_t InitJaCharMap( Kahana_t *kahana )
{
	apr_status_t rc = APR_SUCCESS;
	apr_pool_t *p = NULL;
	kJapaneseCharactorMap_t *sJaCMap = NULL;
	
	if( ( rc = kahanaMalloc( kahana->p, sizeof( kJapaneseCharactorMap_t ), (void**)&sJaCMap, &p ) ) ){
		kahanaLogPut( NULL, NULL, "failed to kahanaMalloc(): %s", STRERROR_APR( rc ) );
	}
	else
	{
		const char *errstr = NULL;
		static const char *hKanaD[] = { "\xef\xbd\xb3\xef\xbe\x9e","\xef\xbd\xb6\xef\xbe\x9e","\xef\xbd\xb7\xef\xbe\x9e","\xef\xbd\xb8\xef\xbe\x9e","\xef\xbd\xb9\xef\xbe\x9e","\xef\xbd\xba\xef\xbe\x9e","\xef\xbd\xbb\xef\xbe\x9e","\xef\xbd\xbc\xef\xbe\x9e","\xef\xbd\xbd\xef\xbe\x9e","\xef\xbd\xbe\xef\xbe\x9e","\xef\xbd\xbf\xef\xbe\x9e","\xef\xbe\x80\xef\xbe\x9e","\xef\xbe\x81\xef\xbe\x9e","\xef\xbe\x82\xef\xbe\x9e","\xef\xbe\x83\xef\xbe\x9e","\xef\xbe\x84\xef\xbe\x9e","\xef\xbe\x8a\xef\xbe\x9e","\xef\xbe\x8a\xef\xbe\x9f","\xef\xbe\x8b\xef\xbe\x9e","\xef\xbe\x8b\xef\xbe\x9f","\xef\xbe\x8c\xef\xbe\x9e","\xef\xbe\x8c\xef\xbe\x9f","\xef\xbe\x8d\xef\xbe\x9e","\xef\xbe\x8d\xef\xbe\x9f","\xef\xbe\x8e\xef\xbe\x9e","\xef\xbe\x8e\xef\xbe\x9f", NULL };
		static const char *zKanaD[] = { "\xe3\x83\xb4","\xe3\x82\xac","\xe3\x82\xae","\xe3\x82\xb0","\xe3\x82\xb2","\xe3\x82\xb4","\xe3\x82\xb6","\xe3\x82\xb8","\xe3\x82\xba","\xe3\x82\xbc","\xe3\x82\xbe","\xe3\x83\x80","\xe3\x83\x82","\xe3\x83\x85","\xe3\x83\x87","\xe3\x83\x89","\xe3\x83\x90","\xe3\x83\x91","\xe3\x83\x93","\xe3\x83\x94","\xe3\x83\x96","\xe3\x83\x97","\xe3\x83\x99","\xe3\x83\x9a","\xe3\x83\x9c","\xe3\x83\x9d", NULL };
		static const char *hKana[] = { "\xef\xbd\xa1","\xef\xbd\xa2","\xef\xbd\xa3","\xef\xbd\xa4","\xef\xbd\xa5","\xef\xbd\xa6","\xef\xbd\xa7","\xef\xbd\xa8","\xef\xbd\xa9","\xef\xbd\xaa","\xef\xbd\xab","\xef\xbd\xac","\xef\xbd\xad","\xef\xbd\xae","\xef\xbd\xaf","\xef\xbd\xb0","\xef\xbd\xb1","\xef\xbd\xb2","\xef\xbd\xb3","\xef\xbd\xb4","\xef\xbd\xb5","\xef\xbd\xb6","\xef\xbd\xb7","\xef\xbd\xb8","\xef\xbd\xb9","\xef\xbd\xba","\xef\xbd\xbb","\xef\xbd\xbc","\xef\xbd\xbd","\xef\xbd\xbe","\xef\xbd\xbf","\xef\xbe\x80","\xef\xbe\x81","\xef\xbe\x82","\xef\xbe\x83","\xef\xbe\x84","\xef\xbe\x85","\xef\xbe\x86","\xef\xbe\x87","\xef\xbe\x88","\xef\xbe\x89","\xef\xbe\x8a","\xef\xbe\x8b","\xef\xbe\x8c","\xef\xbe\x8d","\xef\xbe\x8e","\xef\xbe\x8f","\xef\xbe\x90","\xef\xbe\x91","\xef\xbe\x92","\xef\xbe\x93","\xef\xbe\x94","\xef\xbe\x95","\xef\xbe\x96","\xef\xbe\x97","\xef\xbe\x98","\xef\xbe\x99","\xef\xbe\x9a","\xef\xbe\x9b","\xef\xbe\x9c","\xef\xbe\x9d","\xef\xbe\x9e","\xef\xbe\x9f", NULL };
		static const char *zKana[] = { "\xe3\x80\x82","\xe3\x80\x8c","\xe3\x80\x8d","\xe3\x80\x81","\xe3\x83\xbb","\xe3\x83\xb2","\xe3\x82\xa1","\xe3\x82\xa3","\xe3\x82\xa5","\xe3\x82\xa7","\xe3\x82\xa9","\xe3\x83\xa3","\xe3\x83\xa5","\xe3\x83\xa7","\xe3\x83\x83","\xe3\x83\xbc","\xe3\x82\xa2","\xe3\x82\xa4","\xe3\x82\xa6","\xe3\x82\xa8","\xe3\x82\xaa","\xe3\x82\xab","\xe3\x82\xad","\xe3\x82\xaf","\xe3\x82\xb1","\xe3\x82\xb3","\xe3\x82\xb5","\xe3\x82\xb7","\xe3\x82\xb9","\xe3\x82\xbb","\xe3\x82\xbd","\xe3\x82\xbf","\xe3\x83\x81","\xe3\x83\x84","\xe3\x83\x86","\xe3\x83\x88","\xe3\x83\x8a","\xe3\x83\x8b","\xe3\x83\x8c","\xe3\x83\x8d","\xe3\x83\x8e","\xe3\x83\x8f","\xe3\x83\x92","\xe3\x83\x95","\xe3\x83\x98","\xe3\x83\x9b","\xe3\x83\x9e","\xe3\x83\x9f","\xe3\x83\xa0","\xe3\x83\xa1","\xe3\x83\xa2","\xe3\x83\xa4","\xe3\x83\xa6","\xe3\x83\xa8","\xe3\x83\xa9","\xe3\x83\xaa","\xe3\x83\xab","\xe3\x83\xac","\xe3\x83\xad","\xe3\x83\xaf","\xe3\x83\xb3","\xe3\x82\x9b","\xe3\x82\x9c", NULL };
		static const char *zNum[] = { "\xef\xbc\x90","\xef\xbc\x91","\xef\xbc\x92","\xef\xbc\x93","\xef\xbc\x94","\xef\xbc\x95","\xef\xbc\x96","\xef\xbc\x97","\xef\xbc\x98","\xef\xbc\x99", NULL };
		static const char *hNum[] = { "0","1","2","3","4","5","6","7","8","9", NULL };
		static const char *hAlpha[] = { "A","B","C","D","E","F","G","H","I","J","K","L","M","N","O","P","Q","R","S","T","U","V","W","X","Y","Z","a","b","c","d","e","f","g","h","i","j","k","l","m","n","o","p","q","r","s","t","u","v","w","x","y","z", NULL };
		static const char *zAlpha[] = { "\xef\xbc\xa1","\xef\xbc\xa2","\xef\xbc\xa3","\xef\xbc\xa4","\xef\xbc\xa5","\xef\xbc\xa6","\xef\xbc\xa7","\xef\xbc\xa8","\xef\xbc\xa9","\xef\xbc\xaa","\xef\xbc\xab","\xef\xbc\xac","\xef\xbc\xad","\xef\xbc\xae","\xef\xbc\xaf","\xef\xbc\xb0","\xef\xbc\xb1","\xef\xbc\xb2","\xef\xbc\xb3","\xef\xbc\xb4","\xef\xbc\xb5","\xef\xbc\xb6","\xef\xbc\xb7","\xef\xbc\xb8","\xef\xbc\xb9","\xef\xbc\xba","\xef\xbd\x81","\xef\xbd\x82","\xef\xbd\x83","\xef\xbd\x84","\xef\xbd\x85","\xef\xbd\x86","\xef\xbd\x87","\xef\xbd\x88","\xef\xbd\x89","\xef\xbd\x8a","\xef\xbd\x8b","\xef\xbd\x8c","\xef\xbd\x8d","\xef\xbd\x8e","\xef\xbd\x8f","\xef\xbd\x90","\xef\xbd\x91","\xef\xbd\x92","\xef\xbd\x93","\xef\xbd\x94","\xef\xbd\x95","\xef\xbd\x96","\xef\xbd\x97","\xef\xbd\x98","\xef\xbd\x99","\xef\xbd\x9a", NULL };
		static const char *hSym[] = { "\x27","\x22","\x20","\x7e","\x21","\x23","\x24","\x25","\x26","\x28","\x29","\x2a","\x2b","\x2c","\x2d","\x2e","\x2f","\x3a","\x3b","\x3c","\x3d","\x3e","\x3f","\x40","\x5b","\x5d","\x5e","_","\x60","\x7b","\x7c","\x7d","\x7e","\x5c", NULL };
		static const char *zSym[] = { "\xe2\x80\x99","\xe2\x80\x9d","\xe3\x80\x80","\xe3\x80\x9c","\xef\xbc\x81","\xef\xbc\x83","\xef\xbc\x84","\xef\xbc\x85","\xef\xbc\x86","\xef\xbc\x88","\xef\xbc\x89","\xef\xbc\x8a","\xef\xbc\x8b","\xef\xbc\x8c","\xef\xbc\x8d","\xef\xbc\x8e","\xef\xbc\x8f","\xef\xbc\x9a","\xef\xbc\x9b","\xef\xbc\x9c","\xef\xbc\x9d","\xef\xbc\x9e","\xef\xbc\x9f","\xef\xbc\xa0","\xef\xbc\xbb","\xef\xbc\xbd","\xef\xbc\xbe","\xef\xbc\xbf","\xef\xbd\x80","\xef\xbd\x9b","\xef\xbd\x9c","\xef\xbd\x9d","\xef\xbd\x9e","\xef\xbf\xa5", NULL };
		const char *key,*val;
		int i;
		
		sJaCMap->p = p;
		// create hash map
		sJaCMap->fh = apr_hash_make( p );
		sJaCMap->hk = apr_hash_make( p );
		// 濁点付カタカナ
		for( i = 0; ( key = hKanaD[i] ) && ( val = zKanaD[i] ); i++ ){
			apr_hash_set( sJaCMap->fh, key, APR_HASH_KEY_STRING, val );
			apr_hash_set( sJaCMap->fh, val, APR_HASH_KEY_STRING, key );
		}
		// カタカナ
		for( i = 0; ( key = hKana[i] ) && ( val = zKana[i] ); i++ ){
			apr_hash_set( sJaCMap->fh, key, APR_HASH_KEY_STRING, val );
			apr_hash_set( sJaCMap->fh, val, APR_HASH_KEY_STRING, key );
		}
		// 数字
		for( i = 0; ( key = hNum[i] ) && ( val = zNum[i] ); i++ ){
			apr_hash_set( sJaCMap->fh, key, APR_HASH_KEY_STRING, val );
			apr_hash_set( sJaCMap->fh, val, APR_HASH_KEY_STRING, key );
		}
		// アルファベット
		for( i = 0; ( key = hAlpha[i] ) && ( val = zAlpha[i] ); i++ ){
			apr_hash_set( sJaCMap->fh, key, APR_HASH_KEY_STRING, val );
			apr_hash_set( sJaCMap->fh, val, APR_HASH_KEY_STRING, key );
		}
		// 記号
		for( i = 0; ( key = hSym[i] ) && ( val = zSym[i] ); i++ ){
			apr_hash_set( sJaCMap->fh, key, APR_HASH_KEY_STRING, val );
			apr_hash_set( sJaCMap->fh, val, APR_HASH_KEY_STRING, key );
		}
		
		// create regular expressions
		// half-width
		if( !( rc = kahanaStrRegexpNew( sJaCMap->p, &sJaCMap->h2fSym, REGEXP_SYMBOL, ONIG_OPTION_IGNORECASE, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &errstr ) ) &&
			!( rc = kahanaStrRegexpNew( sJaCMap->p, &sJaCMap->h2fNum, REGEXP_NUMBER, ONIG_OPTION_IGNORECASE, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &errstr ) ) &&
			!( rc = kahanaStrRegexpNew( sJaCMap->p, &sJaCMap->h2fAlpha, REGEXP_ALPHA, ONIG_OPTION_IGNORECASE, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &errstr ) ) &&
			!( rc = kahanaStrRegexpNew( sJaCMap->p, &sJaCMap->h2fAlphaLC, REGEXP_ALPHA_LC, ONIG_OPTION_NONE, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &errstr ) ) &&
			!( rc = kahanaStrRegexpNew( sJaCMap->p, &sJaCMap->h2fAlphaUC, REGEXP_ALPHA_UC, ONIG_OPTION_NONE, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &errstr ) ) &&
			!( rc = kahanaStrRegexpNew( sJaCMap->p, &sJaCMap->h2fKana, REGEXP_KANA_HALF, ONIG_OPTION_IGNORECASE, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &errstr ) ) &&
			!( rc = kahanaStrRegexpNew( sJaCMap->p, &sJaCMap->h2fKanaD, REGEXP_KANA_D_HALF, ONIG_OPTION_IGNORECASE, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &errstr ) ) &&
			// full-width
			!( rc = kahanaStrRegexpNew( sJaCMap->p, &sJaCMap->f2hSym, REGEXP_SYMBOL_FULL, ONIG_OPTION_IGNORECASE, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &errstr ) ) &&
			!( rc = kahanaStrRegexpNew( sJaCMap->p, &sJaCMap->f2hNum, REGEXP_NUMBER_FULL, ONIG_OPTION_IGNORECASE, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &errstr ) ) &&
			!( rc = kahanaStrRegexpNew( sJaCMap->p, &sJaCMap->f2hAlpha, REGEXP_ALPHA_FULL, ONIG_OPTION_IGNORECASE, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &errstr ) ) &&
			!( rc = kahanaStrRegexpNew( sJaCMap->p, &sJaCMap->f2hAlphaLC, REGEXP_ALPHA_LC_FULL, ONIG_OPTION_NONE, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &errstr ) ) &&
			!( rc = kahanaStrRegexpNew( sJaCMap->p, &sJaCMap->f2hAlphaUC, REGEXP_ALPHA_UC_FULL, ONIG_OPTION_NONE, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &errstr ) ) &&
			!( rc = kahanaStrRegexpNew( sJaCMap->p, &sJaCMap->f2hKana, REGEXP_KANA, ONIG_OPTION_IGNORECASE, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &errstr ) ) )
		{
			rc = kahanaStrRegexpNew( sJaCMap->p, &sJaCMap->f2hKanaD, REGEXP_KANA_D, ONIG_OPTION_IGNORECASE, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &errstr );
		}
	}
	
	return rc;
}



const char *kahanaStrJaMapFH( const char *str )
{
	kJapaneseCharactorMap_t *sJaCMap = GetJaCharMap();
	return ( sJaCMap && str ) ? (const char*)apr_hash_get( sJaCMap->fh, str, APR_HASH_KEY_STRING ) : NULL;
}

const char *kahanaStrJaMapHK( const char *str )
{
	kJapaneseCharactorMap_t *sJaCMap = GetJaCharMap();
	return ( sJaCMap && str ) ? (const char*)apr_hash_get( sJaCMap->hk, str, APR_HASH_KEY_STRING ) : NULL;
}

const char *kahanaStrJaReplace( apr_pool_t *p, const char *str, kStrJaCharReplaceType_e type, const char **errstr )
{
	kJapaneseCharactorMap_t *sJaCMap = GetJaCharMap();
	
	if( sJaCMap )
	{
		kRegexp_t *ro = NULL;
		
		switch( type )
		{
			// ALL
			case CONV_H2F:
				if( !( *errstr = kahanaStrRegexpReplaceByCallback( p, sJaCMap->h2fKanaD, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str ) ) &&
					!( *errstr = kahanaStrRegexpReplaceByCallback( p, sJaCMap->h2fKana, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str ) ) &&
					!( *errstr = kahanaStrRegexpReplaceByCallback( p, sJaCMap->h2fNum, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str ) ) &&
					!( *errstr = kahanaStrRegexpReplaceByCallback( p, sJaCMap->h2fAlpha, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str ) ) ){
					*errstr = kahanaStrRegexpReplaceByCallback( p, sJaCMap->h2fSym, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str );
				}
			break;
			case CONV_F2H:
				if( !( *errstr = kahanaStrRegexpReplaceByCallback( p, sJaCMap->f2hKanaD, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str ) ) &&
					!( *errstr = kahanaStrRegexpReplaceByCallback( p, sJaCMap->f2hKana, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str ) ) &&
					!( *errstr = kahanaStrRegexpReplaceByCallback( p, sJaCMap->f2hNum, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str ) ) &&
					!( *errstr = kahanaStrRegexpReplaceByCallback( p, sJaCMap->f2hAlpha, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str ) ) ){
					*errstr = kahanaStrRegexpReplaceByCallback( p, sJaCMap->f2hSym, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str );
				}
			break;
			// NUMBER
			case CONV_H2F_NUMBER:
				ro = sJaCMap->h2fNum;
			break;
			case CONV_F2H_NUMBER:
				ro = sJaCMap->f2hNum;
			break;
			// ALPHABET
			case CONV_H2F_ALPHA:
				ro = sJaCMap->h2fAlpha;
			break;
			case CONV_F2H_ALPHA:
				ro = sJaCMap->f2hAlpha;
			break;
			// ALPHABET LOWER CASE
			case CONV_H2F_ALPHA_LC:
				ro = sJaCMap->h2fAlphaLC;
			break;
			case CONV_F2H_ALPHA_LC:
				ro = sJaCMap->f2hAlphaLC;
			break;
			// ALPHABET UPPER CASE
			case CONV_H2F_ALPHA_UC:
				ro = sJaCMap->h2fAlphaUC;
			break;
			case CONV_F2H_ALPHA_UC:
				ro = sJaCMap->f2hAlphaUC;
			break;
			// SYMBOLS
			case CONV_H2F_SYMBOL:
				ro = sJaCMap->h2fSym;
			break;
			case CONV_F2H_SYMBOL:
				ro = sJaCMap->f2hSym;
			break;
			// KATAKANA_K
			case CONV_H2F_KANA_K:
				ro = sJaCMap->h2fKana;
			break;
			case CONV_F2H_KANA_K:
				ro = sJaCMap->f2hKana;
			break;
			// KATAKANA_D
			case CONV_H2F_KANA_D:
				ro = sJaCMap->h2fKanaD;
			break;
			case CONV_F2H_KANA_D:
				ro = sJaCMap->f2hKanaD;
			break;
			// KATAKANA
			case CONV_H2F_KANA:
				if( !( *errstr = kahanaStrRegexpReplaceByCallback( p, sJaCMap->h2fKanaD, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str ) ) ){
					*errstr = kahanaStrRegexpReplaceByCallback( p, sJaCMap->h2fKana, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str );
				}
			break;
			case CONV_F2H_KANA:
				if( !( *errstr = kahanaStrRegexpReplaceByCallback( p, sJaCMap->f2hKanaD, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str ) ) ){
					*errstr = kahanaStrRegexpReplaceByCallback( p, sJaCMap->f2hKana, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str );
				}
			break;
			
			default:
				*errstr = "failed to kahanaStrJaReplace(): unknown replace type";
				kahanaLogPut( NULL, NULL, "%s", *errstr );
		}
		
		if( ro ){
			*errstr = kahanaStrRegexpReplaceByCallback( p, ro, str, NULL, ONIG_OPTION_NONE, TRUE, cbJaCharMap, NULL, &str );
		}
	}
	
	return str;
}

// MARK:  Full to Half Convert
const char *kahanaStrJaConvString( apr_pool_t *p, const char *str, const char **errstr )
{
	const char *val = NULL;
	int len = ( str ) ? strlen( str ) : 0;
	
	if( len )
	{
		int i;
		int charType;
		const int charTypes[] = {
			CONV_F2H_NUMBER,
			CONV_F2H_ALPHA,
			CONV_F2H_SYMBOL,
			CONV_H2F_KANA,
			0 };
		
		val = str;
		for( i = 0; ( charType = charTypes[i] ); i++ )
		{
			val = kahanaStrJaReplace( p, val, charType, errstr );
			if( *errstr ){
				val = NULL;
				break;
			}
		}
	}
	
	return val;
}

const char *kahanaStrJaConvNumber( apr_pool_t *p, const char *str, long long *num, const char **errstr )
{
	const char *val = NULL;
	int len = ( str ) ? strlen( str ) : 0;
	
	*num = 0;
	if( len )
	{
		val = kahanaStrJaReplace( p, str, CONV_F2H_NUMBER, errstr );
		if( *errstr ){
			val = NULL;
		}
		else
		{
			*num = kahanaStrtoll( val, 10 );
			if( errno ){
				val = NULL;
				*errstr = strerror( errno );
			}
			else
			{
				char asval[255];
				sprintf( asval, "%lld", *num );
				val = (const char*)apr_psprintf( p, "%s", asval );
			}
		}
	}
	
	return val;
}

const char *kahanaStrJaConvRealNumber( apr_pool_t *p, const char *str, long double *num, const char **errstr )
{
	const char *val = NULL;
	int len = ( str ) ? strlen( str ) : 0;
	
	*num = 0;
	if( len )
	{
		int i;
		int charType;
		const int charTypes[] = {
			CONV_F2H_NUMBER,
			CONV_F2H_SYMBOL,
			0 };
		
		val = str;
		for( i = 0; ( charType = charTypes[i] ); i++ )
		{
			val = kahanaStrJaReplace( p, val, charType, errstr );
			if( *errstr ){
				val = NULL;
				break;
			}
		}
		
		if( !*errstr )
		{
			*num = kahanaStrtold( val );
			if( errno ){
				val = NULL;
				*errstr = strerror( errno );
			}
			else {
				val = (const char*)apr_psprintf( p, "%.7f", (double)*num );
			}
		}
	}
	
	return val;
}


// MARK:  String
/*
bit = ( ( bit >> 1 ) & 0x55555555 ) + ( bit & 0x55555555 );
bit = ( ( bit >> 2 ) & 0x33333333 ) + ( bit & 0x33333333 );
bit = ( ( bit >> 4 ) & 0x0f0f0f0f ) + ( bit & 0x0f0f0f0f );
bit = ( ( bit >> 8 ) & 0x00ff00ff ) + ( bit & 0x00ff00ff );
bit = ( bit >> 16 ) + ( bit & 0x0000ffff );
*/
unsigned int kahanaStrByteCount( unsigned char c )
{
	int byte = 1;
	
	if( c <= 0x7f ){
		return 1;
	}
	else if( c <= 0xdf ){
		return 2;
	}
	else if( c <= 0xef ){
		return 3;
	}
	else if( c <= 0xf7 ){
		return 4;
	}
	else if( c <= 0xfb ){
		return 5;
	}
	else if( c <= 0xfd ){
		return 6;
	}

	return byte;
}

size_t kahanaStrlen( const char *str )
{
	char *ptr;
	unsigned int width = 0;
	size_t len = 0;
	
	for( ptr = (char*)str; *ptr != '\0'; ptr += width ){
		width = kahanaStrByteCount( *ptr );
		len++;
	}
	
	return len;
}

size_t kahanaStrPos( const char *str, size_t len )
{
	char *ptr;
	unsigned int width = 0;
	size_t count = 0;
	size_t pos = 0;
	
	for( ptr = (char*)str; *ptr != '\0' && count++ < len; ptr += width ){
		pos += ( width = kahanaStrByteCount( *ptr ) );
	}
	
	return pos;
}

// end of line to LF
void kahanaStrEOLtoLF( char *str )
{
	char *ptr = str;
	size_t len = strlen( ptr );
	
	for( ptr = str; *ptr != '\0'; ptr++ )
	{
		if( ptr[0] == 0xd && ptr[1] == 0xa ){
			memcpy( ptr, ptr + 1, len - 1 );
			ptr[len-1] = '\0';
		}
		else if( ptr[0] == 0xd ){
			*ptr = 0xa;
		}
		len--;
	}
}

const char *kahanaStrtoBytes( apr_pool_t *p, const char *str )
{
	char encstr[strlen(str)*4+1];
	char *dest = encstr;
	char buf[8];
	char *ptr;
	size_t len = 0, width = 0,i;
	
	for( ptr = (char*)str; *ptr != '\0'; ptr += width )
	{
		if( ( width = kahanaStrByteCount( *ptr ) ) == 1 ){
			strncpy( dest, ptr, 1 );
			dest++;
			len++;
		}
		else
		{
			for( i = 0; i < width; i++ ){
				sprintf( buf, "%02X", (unsigned char)ptr[i] );
				strncpy( dest, buf, 2 );
				dest += 2;
				len += 2;
			}
		}
	}
	encstr[len] = '\0';
	return ( len ) ? (const char*)apr_pstrndup( p, encstr, len ) : NULL;

}

long kahanaStrtol( const char *str, int base )
{
	long num = 0;
	
	if( !str ){
		errno = EINVAL;
	}
	else
	{
		char *eptr = NULL;
		
		errno = 0;
		num = strtol( str, &eptr, base );
		if( errno ){
			num = 0;
		}
	}
	
	return num;
}

long long kahanaStrtoll( const char *str, int base )
{
	long long num = 0;
	
	if( !str ){
		errno = EINVAL;
	}
	else
	{
		char *eptr = NULL;
		
		errno = 0;
		num = strtoll( str, &eptr, base );
		if( errno ){
			num = 0;
		}
	}
	
	return num;
}

unsigned long kahanaStrtoul( const char *str, int base )
{
	unsigned long num = 0;
	
	if( !str ){
		errno = EINVAL;
	}
	else
	{
		char *eptr = NULL;
		
		errno = 0;
		num = strtoul( str, &eptr, base );
		if( errno ){
			num = 0;
		}
	}
	
	return num;
}

unsigned long long kahanaStrtoull( const char *str, int base )
{
	unsigned long long num = 0;
	
	if( !str ){
		errno = EINVAL;
	}
	else
	{
		char *eptr = NULL;
		
		errno = 0;
		num = strtoull( str, &eptr, base );
		if( errno ){
			num = 0;
		}
	}
	
	return num;
}

double kahanaStrtod( const char *str )
{
	double num = 0;
	
	if( !str ){
		errno = EINVAL;
	}
	else
	{
		char *eptr = NULL;
		
		errno = 0;
		num = strtod( str, &eptr );
		if( errno ){
			num = 0;
		}
	}
	
	return num;
}
long double kahanaStrtold( const char *str )
{
	long double num = 0;
	
	if( !str ){
		errno = EINVAL;
	}
	else
	{
		char *eptr = NULL;
		
		errno = 0;
		num = strtold( str, &eptr );
		if( errno ){
			num = 0;
		}
	}
	
	return num;
}

// convert upper to lower case
void kahanaStrtoLower( char *str )
{
	char *ptr = str;
	
	while( *ptr != '\0' ){
		*ptr = tolower( *ptr );
		ptr++;
	}
}

void kahanaStrtoUpper( char *str )
{
	char *ptr = str;
	while( *ptr != '\0' ){
		*ptr = toupper( *ptr );
		ptr++;
	}
}

bool kahanaStrCmp( const char *src, const char *exp )
{
	if( src && exp &&
		strcmp( src, exp ) == 0 ){
		return TRUE;
	}
	
	return FALSE;
}

bool kahanaStrCmpHead( const char *src, const char *exp )
{
	size_t slen,elen;
	
	if( src && exp && ( slen = strlen( src ) ) >= ( elen = strlen( exp ) ) ){
		return ( slen == 0 || memcmp( src, exp, elen ) == 0 );
	}
	
	return FALSE;
}

bool kahanaStrCmpTail( const char *src, const char *exp )
{
	size_t len;
	size_t elen;
	
	if( src && exp && ( len = strlen( src ) ) >= ( elen = strlen( exp ) ) ){
		return ( len == 0 || memcmp( src + len - elen, exp, elen ) == 0 );
	}
	
	return FALSE;
}


const char *kahanaStrGetPathParentComponent( apr_pool_t *p, const char *path )
{
	const char *str = ( path ) ? rindex( path, '/' ) : NULL;
	
	if( str )
	{
		str = (const char *)apr_pstrndup( p, path, strlen( path ) - strlen( str ) );
		if( strlen( str ) == 0 ){
			str = "/";
		}
	}
	
	return str;
}

const char *kahanaStrGetPathLastComponent( const char *path )
{
	if( path ){
		const char *last = rindex( path, '/' );
		return last+=1;
	}
	
	return path;
}

/*
static int utf8toucs2( const char* utfstr )
{
	int ucs2 = 0;
	int byte;
	
	if( utfstr[0] == '\0' ){
		return '\0';
	}
	else if( ( (unsigned char)utfstr[0] & 0xf0 ) == 0xe0 ){
		byte = 3;
		ucs2 = utfstr[0] & 0x0f;
		ucs2 = ( ucs2 << 6 ) + ( utfstr[ 1 ] & 0x3f );
		ucs2 = ( ucs2 << 6 ) + ( utfstr[ 2 ] & 0x3f );        
	}
	else if( ( ( unsigned char ) utfstr[ 0 ] & 0x80 ) == 0 ){ // ascii
		byte = 1;
		ucs2 =  utfstr[ 0 ];
	}
	else if( ( ( unsigned char ) utfstr[ 0 ] & 0xe0 ) == 0xc0 ){
		byte = 2;
		ucs2 = utfstr[ 0 ] & 0x1f;
		ucs2 = ( ucs2 << 6 ) + ( utfstr[ 1 ] & 0x3f );
	}
	else if( ( ( unsigned char ) utfstr[ 0 ] & 0xf8 ) == 0xf0 ){
		byte = 4;
		ucs2 = utfstr[ 0 ] & 0x07;
		ucs2 = ( ucs2 << 6 ) + ( utfstr[ 1 ] & 0x3f );
		ucs2 = ( ucs2 << 6 ) + ( utfstr[ 2 ] & 0x3f );
		ucs2 = ( ucs2 << 6 ) + ( utfstr[ 3 ] & 0x3f );        
	}
	// 不正なUTF8
	else {
		byte = 1;
		ucs2 =  utfstr[ 0 ];
		//ERRMSG( "MISC::utf8toucs2 : invalid code = " + MISC::itostr( ucs2 ) );
	}

	return ucs2;
}

static int UTF8toUCS2( const char *str )
{
	int charCode = 0;
	unsigned char *ptr = (unsigned char*)str;
	
	// ascii
	if( ( ptr[0] & 0x80 ) == 0 ){
		charCode = ptr[0];
	}
	// incorrect UTF-8
	else if( ( ptr[0] & 0x40 ) == 0 ){
		charCode = -1;
	}
	else if( ( ptr[0] & 0x60 ) == 0x40 ){
		charCode = ( ( ptr[0] & 0x1F ) << 6 ) | ( ptr[1] & 0x3F );
	}
	else if( ( ptr[0] & 0x70) == 0x60 ){
		charCode = ( ( ptr[0] & 0x0F ) << 12 )
					| ( ( ptr[1] & 0x3F ) << 6 )
					| ( ptr[2] & 0x3F );
	}
	else if( ( ptr[0] & 0x78) == 0x70 ){
		charCode = ((ptr[0] & 0x03 ) << 18 )
			| ( ( ptr[1] & 0x3F ) << 12 )
			| ( ( ptr[2] & 0x3F ) << 6 )
			| ( ( ptr[3] & 0x3F ) );
	}
	else{
		charCode = -1;
	}
	
	kfDEBUG_LOG( "%s -> %d", ptr, charCode );
	
	return charCode;
}
*/

const char *kahanaStrEncodeURI( apr_pool_t *p, const char *str, int component )
{
	static const char no_encode_URI[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789"
		"!#$&'()*+,-./:;=?@_~";
	static const char no_encode_URI_component[] =
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz"
		"0123456789"
		"!'()*-._~";
	const char *doNotEncode = ( component ) ? no_encode_URI_component : no_encode_URI;
	char encstr[strlen(str)*3+1];
	char *dest = encstr;
	char buf[4];
	char *ptr = NULL;
	size_t len = 0;
	
	for( ptr = (char*)str; *ptr != '\0'; ptr++ )
	{
		if( strchr( doNotEncode, *ptr ) ){
			strncpy( dest, ptr, 1 );
			dest++;
			len++;
		}
		else
		{
			sprintf( buf, "%%%02X", (unsigned char)*ptr );
			strncpy( dest, buf, 3 );
			dest += 3;
			len += 3;
		}
	}
	
	return ( len ) ? (const char*)apr_pstrndup( p, encstr, len ) : NULL;
}

const char *kahanaStrDecodeURI( apr_pool_t *p, const char *str, int component )
{
	static const char *no_decode_URI = "#$&+,/:;=?@";
	static const char *no_decode_URI_component = "";
	const char *doNotDecode = ( component ) ? no_decode_URI_component : no_decode_URI;
	size_t len = strlen( str );
	char encstr[len + 1];
	int i = 0, j = 0;
	char c;
	
	for( i = j = 0; str[i] != '\0'; i++ )
	{
		c = str[i];
		if( c == '%' )
		{
			// str[i-i+2] = %[0-9a-fA-F][0-9a-fA-F]
			if( isxdigit( (unsigned char)str[i+1] ) && 
				isxdigit( (unsigned char)str[i+2] ) )
			{
				char buf[] = { str[i+1], str[i+2], '\0' };
				
				c = (char)strtol( buf, NULL, 16 );
				// check is no decode target
				if( strchr( doNotDecode, c ) ){
					c = str[i];
				}
				else {
					i+=2;
				}
			}
			else if( str[i+1] == 'u' &&
					isxdigit( (unsigned char)str[i+2] ) &&
					isxdigit( (unsigned char)str[i+3] ) &&
					isxdigit( (unsigned char)str[i+4] ) &&
					isxdigit( (unsigned char)str[i+5] ) )
			{
				char buf[] = { str[i+2], str[i+3], str[i+4], str[i+5], '\0'};
				c = (char)strtol( buf, NULL, 16 );
				i += 5;
				
				/*
				if( hi < 0xD800  || 0xDFFF < hi )
				{
					sprintf( bp, "%c", hi );
					strncpy( (char*)( dst + dlen ), (char*)buf, bp - buf );
					dlen += bp - buf;
				}
				else
				{
					if( 0xDC00 <= hi )
					{
						// invalid
						warn("U+%04X is an invalid surrogate hi\n", hi);
					}
					else
					{
						i++;
						if(src[i] == '%' && src[i+1] == 'u'
							&& isxdigit(src[i+2]) && isxdigit(src[i+3])
							&& isxdigit(src[i+4]) && isxdigit(src[i+5]))
						{
							strncpy((char *)buf, (char *)(src + i + 2), 4);
							lo = strtol((char *)buf, NULL, 16);
							i += 5;
							if( lo < 0xDC00 || 0xDFFF < lo )
							{
								warn("U+%04X is an invalid lo surrogate", lo);
							}
							else
							{
								lo += 0x10000 + (hi - 0xD800) * 0x400 -  0xDC00;
								sprintf( bp, "%c", lo );
								strncpy((char *)(dst+dlen), (char *)buf, bp - buf);
								dlen += bp - buf;
							}
						}
						else
						{
							warn("lo surrogate is missing for U+%04X", hi);
						}
					}
				}
			*/
			}
		}
		encstr[j++] = c;
	}
	
	return (const char*)apr_pstrndup( p, encstr, j );
}

const char *kahanaStrIconv( apr_pool_t *p, const char *src, const char *from, const char *to, const char **errstr )
{
	apr_status_t rc;
	apr_pool_t *tp = NULL;
	const char *conv = NULL;
	
	if( ( rc = apr_pool_create( &tp, p ) ) ){
		*errstr = apr_psprintf( p, "failed to apr_pool_create(): %s", STRERROR_APR( rc ) );
	}
	else
	{
		iconv_t icv;
		
		if( ( icv = iconv_open( to, from ) ) == (iconv_t)-1 ){
			*errstr = apr_psprintf( p, "failed to iconv_open( to:%s, from:%s ): %s", to, from, strerror( errno ) );
		}
		else
		{
			size_t len = strlen( src );
			size_t buf_len = len * 3;
			char *buf = (char*)apr_pcalloc( tp, buf_len );
			
			conv = buf;
			buf_len--;
			if( iconv( icv, (char**)&src, &len, &buf, &buf_len ) == -1 ){
				*errstr = apr_psprintf( p, "failed to iconv(): %s", strerror( errno ) );
				conv = NULL;
			}
			else{
				conv = (const char*)apr_pstrdup( p, conv );
			}
			iconv_close( icv );
		}
		apr_pool_destroy(tp);
	}
	
	return conv;
}
/* table の内容を aStrFormat でフォーマットして返す */
apr_status_t kahanaStrTable2Str( apr_pool_t *p, const char **dest, apr_table_t *tbl, const char *fmt, const char *sep, bool escape )
{
	apr_status_t rc;
	apr_pool_t *tp = NULL;
	const char *str = "";
	
	if( ( rc = apr_pool_create( &tp, p ) ) == APR_SUCCESS )
	{
		const char *errstr = NULL;
		apr_array_header_t *array = (apr_array_header_t*)apr_table_elts( tbl );
		apr_table_entry_t *entry = (apr_table_entry_t*)array->elts;
		int i;
		
		if( escape )
		{
			const char *key,*val;
			
			/* !!This code not completed!! */
			for( i = 0; i < array->nelts; i++ )
			{
				if( entry[i].key )
				{
					key = NULL;
					val = NULL;
					if( ( errstr = kahanaStrEscapeURI( tp, (const char*)entry[i].key, (const char**)&key ) ) ||
						( errstr = kahanaStrEscapeURI( tp, (const char*)entry[i].val, (const char**)&val ) ) ){
						kahanaLogPut( NULL, NULL, "failed to kahanaStrEscapeURI(): %s", errstr );
						rc = APR_EGENERAL;
						break;
					}
					str = (const char*)apr_pstrcat( p, str, sep, apr_psprintf( tp, fmt, key, val ), NULL );
					//APR_ARRAY_PUSH( table_array, const char* ) = (const char*)apr_psprintf( tp, aStrFormat, entries[i].key, entries[i].val );
				}
			}
		}
		else
		{
			for( i = 0; i < array->nelts; i++ )
			{
				if( entry[i].key ){
					str = (const char*)apr_pstrcat( p, str, sep, apr_psprintf( tp, fmt, entry[i].key, entry[i].val ), NULL );
				}
			}
			str = ( strlen(str) > 1 ) ? str + 1 : NULL;
		}
		apr_pool_destroy( tp );
	}
	
	return rc;
}

const char *kahanaStrSanitize( apr_pool_t *p, const char *src )
{
//	src = kfUtilRegReplace( p, src, "(\t)", "", REG_NEWLINE|REG_EXTENDED, NULL );
//	src = kfUtilRegReplace( p, src, "(\n)", "", REG_NEWLINE|REG_EXTENDED, NULL );
	src = kahanaStrReplace( p, src, "  ", " ", 0 );
	src = kahanaStrReplace( p, src, "&lt;", "<", 0 );
	src = kahanaStrReplace( p, src, "&gt;", ">", 0 );
	src = kahanaStrReplace( p, src, ";;", ";", 0 );
	
	return src;
}

const char *kahanaStrEscapeURI( apr_pool_t *p, const char *src, const char **out )
{
	if( src )
	{
		NEOERR *nerr = neos_url_escape( src, (char**)out, NULL );
		
		if( nerr != STATUS_OK )
		{
			STRING str;
			const char *errstr;
			
			nerr_error_string( nerr, &str );
			errstr = (const char*)apr_pstrdup( p, str.buf );
			string_clear(&str);
			return errstr;
		}
		/*
		*out = kahanaStrReplace( p, *out, "+", "%2b", 0 );
		*out = kahanaStrReplace( p, *out, ",", "%2c", 0 );
		*out = kahanaStrReplace( p, *out, "/", "%2f", 0 );
		*out = kahanaStrReplace( p, *out, ":", "%3a", 0 );
		*out = kahanaStrReplace( p, *out, "=", "%3d", 0 );
		*out = kahanaStrReplace( p, *out, "@", "%40", 0 );
		*out = kahanaStrReplace( p, *out, "&", "%26", 0 );
		*out = kahanaStrReplace( p, *out, "$", "%24", 0 );
		*out = kahanaStrReplace( p, *out, "%20", "+", 0 );
		*/
	}
	
	return NULL;
}


const void* kahanaStrDeleteOverlap( apr_pool_t *p, const char *src, const char *separator, DeleteOverlapReturnAs_e returnAs )
{
	if( src && separator )
	{
		apr_status_t rc;
		apr_pool_t *hp = NULL;
		size_t len = strlen( src );
		char buf[len];
		char *ptr = buf;
		
		memcpy( ptr, src, len );
		ptr[len] = '\0';
		
		if( ( rc = apr_pool_create( &hp, p ) ) ){
			kahanaLogPut( NULL, NULL, "failed to apr_pool_create(): %s", STRERROR_APR( rc ) );
		}
		else
		{
			const char *word = NULL;
			apr_hash_t *hash = apr_hash_make( hp );
			
			if( returnAs == AS_HASH )
			{
				while( ( word = (const char*)apr_strtok( NULL, separator, &ptr ) ) )
				{
					if( word[0] != '\0' && !apr_hash_get( hash, word, APR_HASH_KEY_STRING ) ){
						word = (const char*)apr_pstrdup( hp, word );
						apr_hash_set( hash, word, APR_HASH_KEY_STRING, word );
					}
				}
				return (void*)hash;
			}
			else
			{
				const char *result = NULL;
				
				while( ( word = (const char*)apr_strtok( NULL, separator, &ptr ) ) )
				{
					if( word[0] != '\0' && !apr_hash_get( hash, word, APR_HASH_KEY_STRING ) )
					{
						result = ( result ) ? 
								 (const char*)apr_pstrcat( p, result, separator, word, NULL ) :
								 (const char*)apr_pstrdup( p, word );
						apr_hash_set( hash, word, APR_HASH_KEY_STRING, word );
					}
				}
				
				apr_pool_destroy( hp );
				return ( result ) ? result : src;
			}
		}
	}
	
	return NULL;
}

const char *kahanaStrReplace( apr_pool_t *p, const char *srcStr, const char *targetStr, const char *replaceStr, const int numberOfReplace )
{
	const char *resultStr;
	const char *scanSrc;
	int count = 0, targetLen = 0;
	int NOR = ( numberOfReplace < 0 ) ? 1 : numberOfReplace;
	
	if( !replaceStr ){
		return srcStr;
	}
	
	targetLen = strlen( targetStr );
	resultStr = (const char*)apr_pstrdup( p, srcStr );
	scanSrc = resultStr;
	
	for( count = 0; count < NOR; count++ )
	{
		if( ( scanSrc = strstr( scanSrc, targetStr ) ) == NULL ){
			break;
		}
		
		resultStr = (const char*)apr_pstrcat( p, apr_pstrndup( p, resultStr, strlen( resultStr ) - strlen( scanSrc ) ), replaceStr, scanSrc + targetLen, NULL );
		scanSrc += targetLen;
	}
	
	return resultStr;
}

apr_array_header_t *kahanaStrExtr( apr_pool_t *p, const char *srcStr, const char *startStr, const char *endStr, const int numberOfExtract, const int onlyInside, const int matchAll, const int allowEmpty )
{
	apr_array_header_t *resultArray = NULL;
	const char *extractStr = (const char*)apr_pstrdup( p, srcStr );
	const char *scanSrc = extractStr;
	char *startPoint;
	char *endPoint;
	int count = 0, lenStart = strlen( startStr ), lenEnd = strlen( endStr );
	
	if( srcStr == NULL || startStr == NULL || endStr == NULL ){
		return NULL;
	}
	
	do
	{
		if( ( startPoint = strstr( scanSrc, startStr ) ) == NULL )
		{
			break;
		}
		startPoint += lenStart;
		
		if( ( endPoint = strstr( startPoint, endStr ) ) == NULL )
		{
			break;
		}
		scanSrc = endPoint;
		scanSrc += lenEnd;
		extractStr = (const char*)apr_pstrndup( p, startPoint, strlen( startPoint ) - strlen( endPoint ) );
		
		if( extractStr && ( strlen( extractStr ) || allowEmpty ) )
		{
			if( resultArray == NULL )
			{
				resultArray = apr_array_make( p, 0, sizeof(char*) );
			}
			
			// extract only inside
			if( onlyInside )
			{
				APR_ARRAY_PUSH( resultArray, const char* ) = (const char*)apr_pstrcat( p, extractStr, NULL );
			}
			else
			{
				APR_ARRAY_PUSH( resultArray, const char* ) = (const char*)apr_pstrcat( p, startStr, extractStr, endStr, NULL );
			}
			
			if( strlen( extractStr ) && matchAll )
			{
				scanSrc -= lenEnd;
			}
		}
		
		count++;
		if( numberOfExtract > 0 && count >= numberOfExtract )
		{
			break;
		}
		
	}while( 1 );
	
	return resultArray;
}

/*
apr_array_header_t *StrExtract( request_rec *r, const char *srcStr, const char *startStr, const char *endStr, const int numberOfExtract, const int onlyInside, const int matchAll )
{
	apr_array_header_t *resultArray = NULL;
	const char *extractStr = ap_pstrdup( r->pool, srcStr );
	const char *scanSrc = extractStr;
	char *startPoint;
	char *endPoint;
	int count = 0;
	
	if( srcStr == NULL || startStr == NULL || endStr == NULL )
	{
		return NULL;
	}
	
	do
	{
		if( ( startPoint = strstr( scanSrc, startStr ) ) == NULL )
		{
			break;
		}
		startPoint += strlen( startStr );
		
		if( ( endPoint = strstr( startPoint, endStr ) ) == NULL )
		{
			break;
		}
		scanSrc = endPoint;
		scanSrc += strlen( endStr );
		
		extractStr = ap_pstrndup( r->pool, startPoint, endPoint - startPoint );
		if( extractStr != NULL )
		{
			if( resultArray == NULL )
			{
				resultArray = apr_array_make( r->pool, 1, sizeof(char*) );
			}
			
			// extract only inside
			if( onlyInside )
			{
				*(char **)(ap_push_array( resultArray )) = (const char*)apr_psprintf( r->pool, "%s", extractStr );
			}
			else
			{
				*(char **)(ap_push_array( resultArray )) = (const char*)apr_psprintf( r->pool, "%s%s%s", startStr, extractStr, endStr );
			}
			
			if( matchAll )
			{
				scanSrc -= strlen( endStr );
			}
		}
		 
		count++;
		if( numberOfExtract > 0 && count >= numberOfExtract )
		{
			break;
		}
		
	}while( 1 );
	
	return resultArray;
}
*/




// MARK:  Regexp
/*
	// EMAIL VALIDATION
	// This is BNF from RFC822
	char *esc			= "\\\\";
	char *period		= "\\.";
	char *space			= "\\040";
	char *open_br		= "\\[";
	char *close_br		= "\\]";
	char *nonASCII		= "\\x80-\\xff";
	char *ctrl			= "\\000-\\037";
	char *cr_list		= "\\n\\015";
	char *qtext			= (const char*)apr_psprintf( r->pool, "[^%s%s%s\"]", esc, nonASCII, cr_list );
	char *dtext			= (const char*)apr_psprintf( r->pool, "[^%s%s%s%s%s]", esc, nonASCII, cr_list, open_br, close_br );
	char *quoted_pair	= (const char*)apr_psprintf( r->pool, "%s[^%s]", esc, nonASCII );
	char *atom_char		= (const char*)apr_psprintf( r->pool, "[^(%s)<>@,;:\".%s%s%s%s%s]", space, esc, open_br, close_br, ctrl, nonASCII );
	char *atom			= (const char*)apr_psprintf( r->pool, "%s+(?!%s)", atom_char, atom_char );
	char *quoted_str	= (const char*)apr_psprintf( r->pool, "\"%s*(?:%s%s*)*\"", qtext, quoted_pair, qtext );
	char *word			= (const char*)apr_psprintf( r->pool, "(?:%s|%s)", atom, quoted_str );
	char *domain_ref	= (const char*)apr_psprintf( r->pool, "%s", atom );
	char *domain_lit	= (const char*)apr_psprintf( r->pool, "%s(?:%s|%s)*%s", open_br, dtext, quoted_pair, close_br );
	char *sub_domain	= (const char*)apr_psprintf( r->pool, "(?:%s|%s)", domain_ref, domain_lit );
	
	char *domain		= (const char*)apr_psprintf( r->pool, "%s(?:%s%s)*", sub_domain, period, sub_domain );
	char *local_part	= (const char*)apr_psprintf( r->pool, "%s(?:%s|%s)*", word, word, period );
	char *Addr_spec_re	= (const char*)apr_psprintf( r->pool, "%s@%s", local_part, domain );

kRegexp_t *kahanaStrRegexpNew( apr_pool_t *p, const char *pattern, OnigOptionType option, OnigEncoding enc, OnigSyntaxType *syntax, const char **errstr )

option:      正規表現コンパイル時オプション
	ONIG_OPTION_NONE               オプションなし
	ONIG_OPTION_SINGLELINE         '^' -> '¥A', '$' -> '¥Z'
	ONIG_OPTION_MULTILINE          '.'が改行にマッチする
	ONIG_OPTION_IGNORECASE         曖昧マッチ オン
	ONIG_OPTION_EXTEND             パターン拡張形式
	ONIG_OPTION_FIND_LONGEST       最長マッチ
	ONIG_OPTION_FIND_NOT_EMPTY     空マッチを無視
	ONIG_OPTION_NEGATE_SINGLELINE
		ONIG_SYNTAX_POSIX_BASIC, ONIG_SYNTAX_POSIX_EXTENDED,
		ONIG_SYNTAX_PERL, ONIG_SYNTAX_PERL_NG, ONIG_SYNTAX_JAVAで
		デフォルトで有効なONIG_OPTION_SINGLELINEをクリアする。
	ONIG_OPTION_DONT_CAPTURE_GROUP 名前付き捕獲式集合のみ捕獲
	ONIG_OPTION_CAPTURE_GROUP      名前無し捕獲式集合も捕獲

enc:        文字エンコーディング
	ONIG_ENCODING_ASCII         ASCII
	ONIG_ENCODING_ISO_8859_1    ISO 8859-1
	ONIG_ENCODING_ISO_8859_2    ISO 8859-2
	ONIG_ENCODING_ISO_8859_3    ISO 8859-3
	ONIG_ENCODING_ISO_8859_4    ISO 8859-4
	ONIG_ENCODING_ISO_8859_5    ISO 8859-5
	ONIG_ENCODING_ISO_8859_6    ISO 8859-6
	ONIG_ENCODING_ISO_8859_7    ISO 8859-7
	ONIG_ENCODING_ISO_8859_8    ISO 8859-8
	ONIG_ENCODING_ISO_8859_9    ISO 8859-9
	ONIG_ENCODING_ISO_8859_10   ISO 8859-10
	ONIG_ENCODING_ISO_8859_11   ISO 8859-11
	ONIG_ENCODING_ISO_8859_13   ISO 8859-13
	ONIG_ENCODING_ISO_8859_14   ISO 8859-14
	ONIG_ENCODING_ISO_8859_15   ISO 8859-15
	ONIG_ENCODING_ISO_8859_16   ISO 8859-16
	ONIG_ENCODING_UTF8          UTF-8
	ONIG_ENCODING_UTF16_BE      UTF-16BE
	ONIG_ENCODING_UTF16_LE      UTF-16LE
	ONIG_ENCODING_UTF32_BE      UTF-32BE
	ONIG_ENCODING_UTF32_LE      UTF-32LE
	ONIG_ENCODING_EUC_JP        EUC-JP
	ONIG_ENCODING_EUC_TW        EUC-TW
	ONIG_ENCODING_EUC_KR        EUC-KR
	ONIG_ENCODING_EUC_CN        EUC-CN
	ONIG_ENCODING_SJIS          Shift_JIS
	ONIG_ENCODING_KOI8_R        KOI8-R
	ONIG_ENCODING_CP1251        CP1251
	ONIG_ENCODING_BIG5          Big5
	ONIG_ENCODING_GB18030       GB18030


syntax:     正規表現パターン文法定義
	ONIG_SYNTAX_ASIS              plain text
	ONIG_SYNTAX_POSIX_BASIC       POSIX Basic RE
	ONIG_SYNTAX_POSIX_EXTENDED    POSIX Extended RE
	ONIG_SYNTAX_EMACS             Emacs
	ONIG_SYNTAX_GREP              grep
	ONIG_SYNTAX_GNU_REGEX         GNU regex
	ONIG_SYNTAX_JAVA              Java (Sun java.util.regex)
	ONIG_SYNTAX_PERL              Perl
	ONIG_SYNTAX_PERL_NG           Perl + 名前付き捕獲式集合
	ONIG_SYNTAX_RUBY              Ruby
	ONIG_SYNTAX_DEFAULT           default (== Ruby)
*/

static apr_status_t RegexpCleanup( void *ctx )
{
	kRegexp_t *ro = (kRegexp_t*)ctx;
	
	if( ro && ro->regexp ){
		onig_free( ro->regexp );
		onig_end();
	}
	return APR_SUCCESS;
}

apr_status_t kahanaStrRegexpNew( apr_pool_t *pp, kRegexp_t **newro, const char *pattern, OnigOptionType option, OnigEncoding enc, OnigSyntaxType *syntax, const char **errstr )
{
	int rc;
	apr_pool_t *p = NULL;
	kRegexp_t *ro = NULL;
	
	if( ( rc = kahanaMalloc( pp, sizeof( kRegexp_t ), (void**)&ro, &p ) ) ){
		kahanaLogPut( NULL, NULL, "failed to kahanaMalloc(): %s", STRERROR_APR( rc ) );
	}
	else
	{
		ro->p = p;
		ro->regexp = NULL;
		ro->pattern = (const char*)apr_pstrdup( p, pattern );
		ro->option = option;
		ro->enc = enc;
		ro->syntax = syntax;
		
		// new regexp
		if( ( rc = onig_new( &(ro->regexp), (unsigned char*)pattern, (unsigned char*)pattern + strlen( (char*)pattern ), option, enc, syntax, &(ro->einfo) ) ) != ONIG_NORMAL )
		{
			unsigned char strbuf[ONIG_MAX_ERROR_MESSAGE_LEN];
			
			onig_error_code_to_str( strbuf, rc, &(ro->einfo) );
			kahanaLogPut( NULL, NULL, "failed to onig_new(): %s", (char*)strbuf );
			*errstr = (const char*)apr_pstrdup( pp, (char*)strbuf );
			apr_pool_destroy( p );
			ro = NULL;
		}
		else {
			*newro = ro;
			apr_pool_cleanup_register( p, (void*)ro, RegexpCleanup, RegexpCleanup );
		}
	}
	
	return rc;
}

void kahanaStrRegexpFree( kRegexp_t *ro )
{
	if( ro ){
		apr_pool_destroy( ro->p );
	}
}

bool kahanaStrRegexpMatchBool( apr_pool_t *p, kRegexp_t *ro, const char *str, const char *pattern, OnigOptionType option, const char **errstr )
{
	int rc = ONIG_NORMAL;
	bool match = FALSE;
	OnigRegex regexp = NULL;
	OnigRegion *region;
	OnigErrorInfo einfo;
	unsigned char strbuf[ONIG_MAX_ERROR_MESSAGE_LEN];
	unsigned char *start, *end, *range;
	
	if( ro ){
		regexp = ro->regexp;
	}
	else
	{
		// new regexp
		if( ( rc = onig_new( &regexp, (unsigned char*)pattern, (unsigned char*)pattern + strlen( (char*)pattern ), option, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &einfo ) ) != ONIG_NORMAL )
		{
			onig_error_code_to_str( strbuf, rc, &einfo );
			*errstr = (const char*)apr_pstrdup( p, (char*)strbuf );
			return match;
		}
	}
	
	// search
	region = onig_region_new();
	start = (unsigned char*)str;
	end = (unsigned char*)(str + strlen( (char*)str ));
	range = end;
	if( ( rc = onig_search( regexp, (const unsigned char*)str, end, start, range, region, ONIG_OPTION_NONE ) ) > ONIG_MISMATCH ){
		match = TRUE;
	}
	if( rc < ONIG_MISMATCH ){
		onig_error_code_to_str( strbuf, rc, &einfo );
		*errstr = (const char*)apr_pstrdup( p, (char*)strbuf );
		match = FALSE;
	}
	
	onig_region_free( region, 1 );
	if( !ro ){
		onig_free( regexp );
		onig_end();
	}
	
	return match;
}

const char *kahanaStrRegexpMatch( apr_pool_t *p, kRegexp_t *ro, const char *str, const char *pattern, OnigOptionType option, bool global, apr_array_header_t *matches )
{
	int rc = ONIG_NORMAL;
	OnigRegex regexp = NULL;
	OnigRegion *region;
	OnigErrorInfo einfo;
	const char *errstr = NULL;
	unsigned char strbuf[ONIG_MAX_ERROR_MESSAGE_LEN];
	unsigned char *start, *end, *range;
	int group;
	
	if( ro ){
		regexp = ro->regexp;
	}
	else
	{
		// new regexp
		if( ( rc = onig_new( &regexp, (unsigned char*)pattern, (unsigned char*)pattern + strlen( (char*)pattern ),
							option, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &einfo ) ) != ONIG_NORMAL )
		{
			onig_error_code_to_str( strbuf, rc, &einfo );
			errstr = (const char*)apr_pstrdup( p, (char*)strbuf );
			return errstr;
		}
	}
	
	// search
	region = onig_region_new();
	start = (unsigned char*)str;
	end = (unsigned char*)(str + strlen( (char*)str ));
	range = end;
	while( ( rc = onig_search( regexp, (const unsigned char*)str, end, start, range, region, ONIG_OPTION_NONE ) ) > ONIG_MISMATCH )
	{
		// grouping
		for( group = 0; group < region->num_regs; group++ )
		{
			APR_ARRAY_PUSH( matches, const char* ) = ( region->beg[group] == -1 ) ? NULL :
													(const char*)apr_pstrndup( p, str+region->beg[group], region->end[group] - region->beg[group] );
		}
		if( !global ){
			break;
		}
		start = (unsigned char*)(str + region->end[0] );
	}
	if( rc < ONIG_MISMATCH )
	{
		onig_error_code_to_str( strbuf, rc, &einfo );
		errstr = (const char*)apr_pstrdup( p, (char*)strbuf );
	}
	
	onig_region_free( region, 1 );
	if( !ro ){
		onig_free( regexp );
		onig_end();
	}
	
	return errstr;
}

const char *kahanaStrRegexpSplit( apr_pool_t *p, kRegexp_t *ro, const char *str, const char *pattern, OnigOptionType option, unsigned int maxsplit, apr_array_header_t *array )
{
	int rc = ONIG_NORMAL;
	OnigRegex regexp = NULL;
	OnigRegion *region;
	OnigErrorInfo einfo;
	const char *errstr = NULL;
	unsigned char strbuf[ONIG_MAX_ERROR_MESSAGE_LEN];
	unsigned char *start, *end, *range;
	unsigned int count, ps, pe;
	
	if( ro ){
		regexp = ro->regexp;
	}
	else
	{
		// new regexp
		if( ( rc = onig_new( &regexp, (unsigned char*)pattern, (unsigned char*)pattern + strlen( (char*)pattern ),
							option, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &einfo ) ) != ONIG_NORMAL )
		{
			onig_error_code_to_str( strbuf, rc, &einfo );
			errstr = (const char*)apr_pstrdup( p, (char*)strbuf );
			return errstr;
		}
	}
	
	// search
	region = onig_region_new();
	start = (unsigned char*)str;
	end = (unsigned char*)(str + strlen( (char*)str ));
	range = end;
	ps = pe = count = 0;
	while( ( rc = onig_search( regexp, (const unsigned char*)str, end, start, range, region, ONIG_OPTION_NONE ) ) > ONIG_MISMATCH )
	{
		count++;
		if( ps != region->beg[0] )
		{
			APR_ARRAY_PUSH( array, const char* ) = (const char*)apr_pstrndup( p, str+ps, region->beg[0] );
		}
		// grouping
		if( region->num_regs )
		{
			int i;
			for( i = 1; i < region->num_regs; i++ )
			{
				APR_ARRAY_PUSH( array, const char* ) = (const char*)apr_pstrndup( p, str+region->beg[i], region->end[i] - region->beg[i] );
			}
		}
		
		start = (unsigned char*)(str + region->end[0] );
		ps = region->end[0];
		if( maxsplit != 0 && count >= maxsplit ){
			break;
		}
	}
	if( rc < ONIG_MISMATCH )
	{
		onig_error_code_to_str( strbuf, rc, &einfo );
		errstr = (const char*)apr_pstrdup( p, (char*)strbuf );
	}
	else if( rc >= ONIG_MISMATCH && array->nelts )
	{
		APR_ARRAY_PUSH( array, const char* ) = (const char*)apr_pstrndup( p, str+ps, (int)end );
	}
	
	onig_region_free( region, 1 );
	
	if( !ro ){
		onig_free( regexp );
		onig_end();
	}
	
	return errstr;
}

/*
const char *kahanaStrRegexpReplace( apr_pool_t *p, kRegexp_t *ro, const char *str, const char *pattern, OnigOptionType option, bool global, kahanaStrRegexpReplaceCallback callback, const char **errstr )
{
	if( callback )
	{
		int status = ONIG_NORMAL;
		OnigRegex regexp = NULL;
		OnigRegion *region;
		OnigErrorInfo einfo;
		BufferObj *src;
		BufferObj *match;
		const char *ptr, *rep;
		unsigned char *start, *end;
		size_t srcSize = strlen( str );
		size_t len;
		int group, cur;
		
		if( ro ){
			regexp = ro->regexp;
		}
		else
		{
			// new regexp
			if( ( status = onig_new( &regexp, (unsigned char*)pattern, (unsigned char*)pattern + strlen( (char*)pattern ),
								option, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &einfo ) ) != ONIG_NORMAL )
			{
				unsigned char strbuf[ONIG_MAX_ERROR_MESSAGE_LEN];
				
				onig_error_code_to_str( strbuf, status, &einfo );
				*errstr = (const char*)apr_pstrdup( p, strbuf );
				kfDEBUG_LOG( "ERROR: %s", *errstr );
				return str;
			}
		}
		
		// create match conainer and string src
		match = kfBufferInit( p, sizeof( char* ), MAX_STRING_LEN, errstr );
		src = kfBufferInit( p, sizeof( char* ), srcSize, errstr );
		ptr = (const char*)kfBufferStrSet( src, str );
		// create match region
		region = onig_region_new();
		start = (unsigned char*)ptr;
		end = (unsigned char*)( ptr + srcSize );
		while( ( status = onig_search( regexp, (const unsigned char*)kfBufferPtr( src ), end, start, end, region, ONIG_OPTION_NONE ) ) > ONIG_MISMATCH )
		{
			// next starting point
			cur = 0;
			// grouping
			// matched range = region->beg[0] to region->end[0]
			for( group = 0; group < region->num_regs; group++ )
			{
				// if len > 0
				// set match string and
				// get charactor for replace
				if( ( len = region->end[group] - region->beg[group] ) &&
					( rep = (char*)callback( group, (const char*)kfBufferStrNSet( match, ptr + region->beg[group] + cur, len ) ) ) )
				{
					// replace
					kfBufferStrReplaceFromTo( src, region->beg[group] + cur, region->beg[group] + cur + len, rep );
					cur += strlen( rep ) - len;
					// break if group[0] = all match was replaced
					if( group == 0 ){
						break;
					}
				}
			}
			
			if( !global ){
				break;
			}
			start = (unsigned char*)( ptr + region->end[0] + cur );
			end = (unsigned char*)( ptr + kfBufferSize( src ) );
		}
		kfBufferFree( match );

		if( status < ONIG_MISMATCH )
		{
			unsigned char strbuf[ONIG_MAX_ERROR_MESSAGE_LEN];
			
			onig_error_code_to_str( strbuf, status, &einfo );
			*errstr = (const char*)apr_pstrdup( p, strbuf );
			kfDEBUG_LOG( "ERROR: %s", *errstr );
		}
		
		str = (const char*)apr_pstrdup( p, kfBufferPtr( src ) );
		kfBufferFree( src );
		onig_region_free( region, 1 );
		
		if( !ro ){
			onig_free( regexp );
			onig_end();
		}
	}
	
	return str;
}
*/

const char *kahanaStrRegexpReplace( apr_pool_t *p, kRegexp_t *ro, const char *str, const char *pattern, OnigOptionType option, bool global, const char *rep, const char **dest )
{
	apr_status_t rc;
	const char *errstr = NULL;
	kBuffer_t *bo = NULL;
	
	if( !str ){
		errstr = "failed to kahanaStrRegexpReplace(): str is NULL";
		kahanaLogPut( NULL, NULL, "%s", errstr );
	}
	else if( !ro && !pattern ){
		errstr = "failed to kahanaStrRegexpReplace(): pattern is NULL";
		kahanaLogPut( NULL, NULL, "%s", errstr );
	}
	else if( ( rc = kahanaBufCreate( &bo, p, sizeof( char ), strlen( str ) * 2 ) ) ){
		errstr = strerror( rc );
	}
	else if( ( rc = kahanaBufStrSet( bo, str ) ) ){
		errstr = strerror( rc );
		kahanaBufDestroy( bo );
	}
	else
	{
		rep = ( rep ) ? rep : "";
		int status = ONIG_NORMAL;
		OnigRegex regexp = NULL;
		OnigRegion *region;
		OnigErrorInfo einfo;
		const char *ptr = kahanaBufPtr( bo );
		unsigned char *start, *end;
		size_t len = strlen( rep );
		
		if( ro ){
			regexp = ro->regexp;
		}
		else
		{
			// new regexp
			if( ( status = onig_new( &regexp, (unsigned char*)pattern, (unsigned char*)pattern + strlen( (char*)pattern ),
								option, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &einfo ) ) != ONIG_NORMAL )
			{
				unsigned char strbuf[ONIG_MAX_ERROR_MESSAGE_LEN];
				
				onig_error_code_to_str( strbuf, status, &einfo );
				kahanaLogPut( NULL, NULL, "failed to onig_new(): %s", (char*)strbuf );
				errstr = (const char*)apr_pstrdup( p, (char*)strbuf );
				kahanaBufDestroy( bo );
				return errstr;
			}
		}
		
		// create match region
		region = onig_region_new();
		start = (unsigned char*)ptr;
		end = (unsigned char*)( ptr + kahanaBufSize( bo ) );
		while( ( status = onig_search( regexp, (const unsigned char*)ptr, end, start, end, region, ONIG_OPTION_NONE ) ) > ONIG_MISMATCH )
		{
			// replace
			if( ( rc = kahanaBufStrReplaceFromTo( bo, region->beg[0], region->end[0], rep ) ) ){
				errstr = strerror( rc );
				break;
			}
			else {
				ptr = kahanaBufPtr( bo );
				end = (unsigned char*)( ptr + kahanaBufSize( bo ) );
				start = (unsigned char*)( ptr + region->beg[0] + len );
			}
		}
		
		if( !errstr )
		{
			if( status < ONIG_MISMATCH )
			{
				unsigned char strbuf[ONIG_MAX_ERROR_MESSAGE_LEN];
				
				onig_error_code_to_str( strbuf, status, &einfo );
				kahanaLogPut( NULL, NULL, "failed to onig_new(): %s", (char*)strbuf );
				errstr = (const char*)apr_pstrdup( p, (char*)strbuf );
			}
			else {
				*dest = (const char*)apr_pstrdup( p, kahanaBufPtr( bo ) );
			}
		}
		kahanaBufDestroy( bo );
		onig_region_free( region, 1 );
		if( !ro ){
			onig_free( regexp );
			onig_end();
		}
	}
	
	return errstr;
}


const char *kahanaStrRegexpReplaceByCallback( apr_pool_t *p, kRegexp_t *ro, const char *str, const char *pattern, OnigOptionType option, bool global, kahanaStrRegexpReplaceCallback callback, void *ctx, const char **dest )
{
	apr_status_t rc;
	kBuffer_t *bo = NULL;
	const char *errstr = NULL;
	
	if( !str ){
		errstr = "failed to kahanaStrRegexpReplaceByCallback(): str is NULL";
		kahanaLogPut( NULL, NULL, "%s", errstr );
	}
	else if( !ro && !pattern ){
		errstr = "failed to kahanaStrRegexpReplaceByCallback(): pattern is NULL";
		kahanaLogPut( NULL, NULL, "%s", errstr );
	}
	else if( !callback ){
		errstr = "failed to kahanaStrRegexpReplaceByCallback(): callback is NULL";
		kahanaLogPut( NULL, NULL, "%s", errstr );
	}
	// create string buffer object
	else if( ( rc = kahanaBufCreate( &bo, p, sizeof( char ), 0 ) ) ){
		errstr = strerror( rc );
	}
	else if( ( rc = kahanaBufStrSet( bo, str ) ) ){
		errstr = strerror( rc );
		kahanaBufDestroy( bo );
	}
	else
	{
		int status = ONIG_NORMAL;
		OnigRegex regexp = NULL;
		OnigRegion *region;
		OnigErrorInfo einfo;
		const char *ptr, *rep;
		unsigned char *start, *end;
		size_t bytesPtr = sizeof( char* );
		size_t bytes = sizeof( char );
		size_t len;
		size_t match;
		
		if( ro ){
			regexp = ro->regexp;
		}
		else
		{
			// new regexp
			if( ( status = onig_new( &regexp, (unsigned char*)pattern, (unsigned char*)pattern + strlen( (char*)pattern ),
								option, ONIG_ENCODING_UTF8, ONIG_SYNTAX_PERL, &einfo ) ) != ONIG_NORMAL )
			{
				unsigned char strbuf[ONIG_MAX_ERROR_MESSAGE_LEN];
				
				onig_error_code_to_str( strbuf, status, &einfo );
				kahanaLogPut( NULL, NULL, "failed to onig_new(): %s", (char*)strbuf );
				errstr = (const char*)apr_pstrdup( p, (char*)strbuf );
				kahanaBufDestroy( bo );
				return errstr;
			}
		}
		
		ptr = kahanaBufPtr( bo );
		// create match region
		region = onig_region_new();
		start = (unsigned char*)ptr;
		end = (unsigned char*)kahanaBufSize( bo );
		while( ( status = onig_search( regexp, (const unsigned char*)ptr, end, start, end, region, ONIG_OPTION_NONE ) ) > ONIG_MISMATCH )
		{
			int cur = region->end[0];
			char **group = calloc( bytesPtr, region->num_regs + 1 );
			
			// set match string array
			for( match = 0; match < region->num_regs; match++ ){
				len = region->end[match] - region->beg[match];
				group[match] = calloc( bytes, len + 1 );
				memcpy( group[match], ptr + region->beg[match], len );
			}
			
			// get charactor for replace
			if( ( rep = (char*)callback( region->num_regs, (const char**)group, ctx ) ) ){
				// replace
				cur = region->beg[0] + strlen( rep );
				if( ( rc = kahanaBufStrReplaceFromTo( bo, region->beg[0], region->end[0], rep ) ) ){
					errstr = strerror( rc );
					break;
				}
				ptr = kahanaBufPtr( bo );
			}
			
			// free
			for( match = 0; match < region->num_regs; match++ ){
				free( group[match] );
			}
			free( group );
			
			end = (unsigned char*)( ptr + kahanaBufSize( bo ) );
			start = (unsigned char*)( ptr + cur );
		}
		
		if( !errstr )
		{
			if( status < ONIG_MISMATCH )
			{
				unsigned char strbuf[ONIG_MAX_ERROR_MESSAGE_LEN];
				onig_error_code_to_str( strbuf, status, &einfo );
				errstr = (const char*)apr_pstrdup( p, (char*)strbuf );
			}
			else {
				*dest = (const char*)apr_pstrdup( p, kahanaBufPtr( bo ) );
			}
		}
		kahanaBufDestroy( bo );
		onig_region_free( region, 1 );
		if( !ro ){
			onig_free( regexp );
			onig_end();
		}
	}
	
	return str;
}


apr_status_t _kahanaStrInit( void )
{
	Kahana_t *kahana = _kahanaGlobal();
	
	kahana->jaMap = NULL;
	
	return InitJaCharMap( kahana );
}

void _kahanaStrCleanup( void )
{
}