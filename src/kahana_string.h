/*
 *  kahana_string.h
 *
 *  Created by Mah on 06/12/16.
 *  Copyright 2006 Masatoshi Teruya All rights reserved.
 *
 */

#ifndef ___KAHANA_STRING___
#define ___KAHANA_STRING___

#include "kahana.h"

// Regular Expressions
/*
	addr_spec(perl):
		my $wsp = '[\x20\x09]';
		my $vchar = '[\x21-\x7e]';
		my $quoted_pair = "(?:\\\\(?:$vchar|$wsp))";
		my $qtext = '[\x21\x23-\x5b\x5d-\x7e]';
		my $qcontent = "(?:$qtext|$quoted_pair)";
		my $quoted_string = "(?:\"$qcontent*\")";
		# my $atext = '[a-zA-Z0-9!#$%&\'*+\-\/\=?^_`{|}~]';
		my $atext = qq{[a-zA-Z0-9!#\$%&'*+\\-/=?^_`{|}~]};
		my $dot_atom_text = "(?:$atext+(?:\\.$atext+)*)";
		my $dot_atom = $dot_atom_text;
		my $local_part = "(?:$dot_atom|$quoted_string)";
		my $domain = $dot_atom;
		my $addr_spec = qq{$local_part\@$domain};
		
(?:(?:[a-zA-Z0-9!#$%&'*+\-/=?^_`{|}~]+(?:\.[a-zA-Z0-9!#$%&'*+\-/=?^_`{|}~]+)*)|(?:"(?:[\x21\x23-\x5b\x5d-\x7e]|(?:\\(?:[\x21-\x7e]|[\x20\x09])))*"))@(?:[a-zA-Z0-9!#$%&'*+\-/=?^_`{|}~]+(?:\.[a-zA-Z0-9!#$%&'*+\-/=?^_`{|}~]+)*)

	addr_spec_loose(perl):
		my $dot_atom_loose   = "(?:$atext+(?:\\.|$atext)*)";
		my $local_part_loose = "(?:$dot_atom_loose|$quoted_string)";
		my $addr_spec_loose  = qq{$local_part_loose\@$domain};
		
(?:(?:[a-zA-Z0-9!#$%&'*+\-/=?^_`{|}~]+(?:\.|[a-zA-Z0-9!#$%&'*+\-/=?^_`{|}~])*)|(?:"(?:[\x21\x23-\x5b\x5d-\x7e]|(?:\\(?:[\x21-\x7e]|[\x20\x09])))*"))@(?:[a-zA-Z0-9!#$%&'*+\-/=?^_`{|}~]+(?:\.[a-zA-Z0-9!#$%&'*+\-/=?^_`{|}~]+)*)

*/
#define REGEXP_EMAIL_VALID "(?:(?:[a-zA-Z0-9!#$%&'*+\\-/=?^_`{|}~]+(?:\\.[a-zA-Z0-9!#$%&'*+\\-/=?^_`{|}~]+)*)|(?:\"(?:[\x21\x23-\x5b\x5d-\x7e]|(?:\\\\(?:[\x21-\x7e]|[\x20\x09])))*\"))@(?:[a-zA-Z0-9!#$%&'*+\\-/=?^_`{|}~]+(?:\\.[a-zA-Z0-9!#$%&'*+\\-/=?^_`{|}~]+)+)"
#define REGEXP_EMAIL_LOOSE "(?:(?:[a-zA-Z0-9!#$%&'*+\\-/=?^_`{|}~]+(?:\\.|[a-zA-Z0-9!#$%&'*+\\-/=?^_`{|}~])*)|(?:\"(?:[\x21\x23-\x5b\x5d-\x7e]|(?:\\\\(?:[\x21-\x7e]|[\x20\x09])))*\"))@(?:[a-zA-Z0-9!#$%&'*+\\-/=?^_`{|}~]+(?:\\.[a-zA-Z0-9!#$%&'*+\\-/=?^_`{|}~]+)+)"

// half-width number
#define	REGEXP_NUMBER "[0-9]"
// full-width number
#define REGEXP_NUMBER_FULL "[\xef\xbc\x90-\xef\xbc\x99]"
// alphabet
#define REGEXP_ALPHA "[a-zA-Z]"
// lower-case alphabet
#define REGEXP_ALPHA_LC "[a-z]"
// upper-case alphabet
#define REGEXP_ALPHA_UC "[A-Z]"
// full-width alphabet
#define REGEXP_ALPHA_FULL "[\xef\xbd\x81-\xef\xbd\x9a\xef\xbc\xa1-\xef\xbc\xba]"
// full-width lower-case alphabet
#define REGEXP_ALPHA_LC_FULL "[\xef\xbd\x81-\xef\xbd\x9a]"
// full-width upper-case alphabet
#define REGEXP_ALPHA_UC_FULL "[\xef\xbc\xa1-\xef\xbc\xba]"
// half-width symbol
#define REGEXP_SYMBOL "[_\x20-\x2f\x3a-\x40\x5b-\x5e\x60\x7b-\x7e]"
// full-width symbol
#define REGEXP_SYMBOL_FULL "[\xe2\x80\x99\xe2\x80\x9d\xe3\x80\x80\xe3\x80\x9c\xef\xbc\x81\xef\xbc\x83-\xef\xbc\x86\xef\xbc\x88-\xef\xbc\x8f\xef\xbc\x9a-\xef\xbc\xa0\xef\xbc\xbb\xef\xbc\xbd-\xef\xbc\xbf\xef\xbd\x80\xef\xbd\x9b-\xef\xbd\x9e\xef\xbf\xa5]"
// half-width katakana dakuon
#define REGEXP_KANA_D_HALF "(?:[\xef\xbd\xb3\xef\xbd\xb6-\xef\xbd\xbf\xef\xbe\x80-\xef\xbe\x84\xef\xbe\x8a-\xef\xbe\x8e]\xef\xbe\x9e|[\xef\xbe\x8a-\xef\xbe\x8e]\xef\xbe\x9f)"
// full-width katakana dakuon
#define REGEXP_KANA_D "[\xe3\x83\xb4\xe3\x82\xac\xe3\x82\xae\xe3\x82\xb0\xe3\x82\xb2\xe3\x82\xb4\xe3\x82\xb6\xe3\x82\xb8\xe3\x82\xba\xe3\x82\xbc\xe3\x82\xbe\xe3\x83\x80\xe3\x83\x82\xe3\x83\x85\xe3\x83\x87\xe3\x83\x89\xe3\x83\x90\xe3\x83\x91\xe3\x83\x93\xe3\x83\x94\xe3\x83\x96\xe3\x83\x97\xe3\x83\x99\xe3\x83\x9a\xe3\x83\x9c\xe3\x83\x9d]"
// half-width katakana
#define REGEXP_KANA_HALF "[\xef\xbd\xa1-\xef\xbd\xbf\xef\xbe\x80-\xef\xbe\x9f]"
// full-width katakana
#define REGEXP_KANA "[\xe3\x80\x81\xe3\x80\x82\xe3\x80\x8c\xe3\x80\x8d\xe3\x82\x9b\xe3\x82\x9c\xe3\x82\xa1-\xe3\x82\xaf\xe3\x82\xb1\xe3\x82\xb3\xe3\x82\xb5\xe3\x82\xb7\xe3\x82\xb9\xe3\x82\xbb\xe3\x82\xbd\xe3\x82\xbf\xe3\x83\x81\xe3\x83\x83\xe3\x83\x84\xe3\x83\x86\xe3\x83\x88\xe3\x83\x8a-\xe3\x83\x8f\xe3\x83\x92\xe3\x83\x95\xe3\x83\x98\xe3\x83\x9b\xe3\x83\x9e\xe3\x83\x9f\xe3\x83\xa0-\xe3\x83\xad\xe3\x83\xaf\xe3\x83\xb2\xe3\x83\xb3\xe3\x83\xbb\xe3\x83\xbc]"

#define kahanaStrI2S( p, num ) (\
{\
	char _asval[255]; \
	sprintf( _asval, "%d", num ); \
	(const char*)apr_pstrdup( p, _asval ); \
})
#define kahanaStrUI2S( p, num ) (\
{\
	char _asval[255]; \
	sprintf( _asval, "%u", num ); \
	(const char*)apr_pstrdup( p, _asval ); \
})
#define kahanaStrS2S( p, num ) (\
{\
	char _asval[255]; \
	sprintf( _asval, "%hd", num ); \
	(const char*)apr_pstrdup( p, _asval ); \
})
#define kahanaStrUS2S( p, num ) (\
{\
	char _asval[255]; \
	sprintf( _asval, "%hu", num ); \
	(const char*)apr_pstrdup( p, _asval ); \
})
#define kahanaStrL2S( p, num ) (\
{\
	char _asval[255]; \
	sprintf( _asval, "%ld", num ); \
	(const char*)apr_pstrdup( p, _asval ); \
})
#define kahanaStrUL2S( p, num ) (\
{\
	char _asval[255]; \
	sprintf( _asval, "%lu", num ); \
	(const char*)apr_pstrdup( p, _asval ); \
})
#define kahanaStrLL2S( p, num ) (\
{\
	char _asval[255]; \
	sprintf( _asval, "%lld", num ); \
	(const char*)apr_pstrdup( p, _asval ); \
})
#define kahanaStrULL2S( p, num ) (\
{\
	char _asval[255]; \
	sprintf( _asval, "%llu", num ); \
	(const char*)apr_pstrdup( p, _asval ); \
})
#define kahanaStrD2S( p, num ) (\
{\
	char _asval[255]; \
	sprintf( _asval, "%f", num ); \
	(const char*)apr_pstrdup( p, _asval ); \
})
#define kahanaStrLD2S( p, num ) (\
{\
	char _asval[255]; \
	sprintf( _asval, "%Lf", num ); \
	(const char*)apr_pstrdup( p, _asval ); \
})

typedef enum {
	CONV_H2F = 1,
	CONV_F2H,
	CONV_H2F_NUMBER,
	CONV_F2H_NUMBER,
	CONV_H2F_ALPHA,
	CONV_F2H_ALPHA,
	CONV_H2F_ALPHA_LC,
	CONV_F2H_ALPHA_LC,
	CONV_H2F_ALPHA_UC,
	CONV_F2H_ALPHA_UC,
	CONV_H2F_SYMBOL,
	CONV_F2H_SYMBOL,
	CONV_H2F_KANA_K,
	CONV_F2H_KANA_K,
	CONV_H2F_KANA_D,
	CONV_F2H_KANA_D,
	CONV_H2F_KANA,
	CONV_F2H_KANA
} kStrJaCharReplaceType_e;

typedef enum {
	AS_STRING = 0,
	AS_HASH
} DeleteOverlapReturnAs_e;


// Regexp
typedef struct {
	apr_pool_t *p;
	OnigRegex regexp;
	const char *pattern;
	OnigOptionType option;
	OnigEncoding enc;
	OnigSyntaxType *syntax;
	OnigErrorInfo einfo;
} kRegexp_t;
typedef const char *(*kahanaStrRegexpReplaceCallback)( size_t match, const char **group, void *ctx );


// MARK:  Japanese CodeMap
/* Japanese Hiragana-Katakana CharactorMap */
const char *kahanaStrJaMapFH( const char *str );
const char *kahanaStrJaMapHK( const char *str );
const char *kahanaStrJaReplace( apr_pool_t *p, const char *str, kStrJaCharReplaceType_e type, const char **errstr );
/* Convertor */
const char *kahanaStrJaConvString( apr_pool_t *p, const char *str, const char **errstr );
const char *kahanaStrJaConvNumber( apr_pool_t *p, const char *str, long long *num, const char **errstr );
const char *kahanaStrJaConvRealNumber( apr_pool_t *p, const char *str, long double *num, const char **errstr );


// MARK:  STRING
unsigned int kahanaStrByteCount( unsigned char c );
size_t kahanaStrlen( const char *str );
size_t kahanaStrPos( const char *str, size_t len );
void kahanaStrEOLtoLF( char *str );

const char *kahanaStrtoBytes( apr_pool_t *p, const char *str );
long kahanaStrtol( const char *str, int base );
long long kahanaStrtoll( const char *str, int base );
unsigned long kahanaStrtoul( const char *str, int base );
unsigned long long kahanaStrtoull( const char *str, int base );
double kahanaStrtod( const char *str );
long double kahanaStrtold( const char *str );

void kahanaStrtoLower( char *str );
void kahanaStrtoUpper( char *str );

bool kahanaStrCmp( const char *src, const char *exp );
bool kahanaStrCmpHead( const char *src, const char *exp );
bool kahanaStrCmpTail( const char *src, const char *exp );

const char *kahanaStrGetPathParentComponent( apr_pool_t *p, const char *path );
const char *kahanaStrGetPathLastComponent( const char *path );

const char *kahanaStrEncodeURI( apr_pool_t *p, const char *str, int component );
const char *kahanaStrDecodeURI( apr_pool_t *p, const char *str, int component );

const char *kahanaStrIconv( apr_pool_t *p, const char *src, const char *from, const char *to, const char **errstr );

apr_status_t kahanaStrTable2Str( apr_pool_t *p, const char **dest, apr_table_t *tbl, const char *fmt, const char *sep, bool escape );
const char* kahanaStrSanitize( apr_pool_t *p, const char *src );
const char *kahanaStrEscapeURI( apr_pool_t *p, const char *src, const char **out );
const void* kahanaStrDeleteOverlap( apr_pool_t *p, const char *src, const char *separator, DeleteOverlapReturnAs_e returnAs );


const char* kahanaStrReplace( apr_pool_t *p, const char *srcStr, const char *targetStr, const char *replaceStr, const int numberOfReplace );
apr_array_header_t* kahanaStrExtr( apr_pool_t *p, const char *srcStr, const char *startStr, const char *endStr, const int numberOfExtract, const int onlyInside, const int matchAll, const int allowEmpty );


// MARK:  Regexp
apr_status_t kahanaStrRegexpNew( apr_pool_t *pp, kRegexp_t **newro, const char *pattern, OnigOptionType option, OnigEncoding enc, OnigSyntaxType *syntax, const char **errstr );
void kahanaStrRegexpFree( kRegexp_t *ro );
bool kahanaStrRegexpMatchBool( apr_pool_t *p, kRegexp_t *ro, const char *str, const char *pattern, OnigOptionType option, const char **errstr );
const char *kahanaStrRegexpMatch( apr_pool_t *p, kRegexp_t *ro, const char *str, const char *pattern, OnigOptionType option, bool global, apr_array_header_t *matches );
const char *kahanaStrRegexpSplit( apr_pool_t *p, kRegexp_t *ro, const char *str, const char *pattern, OnigOptionType option, unsigned int maxsplit, apr_array_header_t *array );
const char *kahanaStrRegexpReplace( apr_pool_t *p, kRegexp_t *ro, const char *str, const char *pattern, OnigOptionType option, bool global, const char *rep, const char **dest );
const char *kahanaStrRegexpReplaceByCallback( apr_pool_t *p, kRegexp_t *ro, const char *str, const char *pattern, OnigOptionType option, bool global, kahanaStrRegexpReplaceCallback callback, void *ctx, const char **dest );


#endif

