/*
 *  kahana_cmd.h
 *  mod_kahana
 */

#ifndef ___KAHANA_COMMAND___
#define ___KAHANA_COMMAND___

#include "kahana.h"

typedef struct {
	apr_pool_t *p;
	apr_procattr_t *attr;
	apr_file_t *in;
	apr_file_t *out;
	apr_file_t *err;
	int errcode;
	const char *errstr;
} kCmdAttr_t;

/*
// use the shell to invoke the program
APR_SHELLCMD
// invoke the program directly, no copied env
APR_PROGRAM
// < invoke the program, replicating our environment
APR_PROGRAM_ENV
// find program on PATH, use our environment
APR_PROGRAM_PATH
// use the shell to invoke the program, replicating our environment
APR_SHELLCMD_ENV
*/
apr_status_t kahanaCmdNewAttr( kCmdAttr_t **newattr, apr_pool_t *p, apr_cmdtype_e cmdtype );
void kahanaCmdDestroyAttr( kCmdAttr_t *attr );
apr_status_t kahanaCmdRunProcess( kCmdAttr_t *attr, const char *program, const char * const *args, const char * const *env );


#endif

