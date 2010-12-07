#ifndef ___KAHANA_MODULE___
#define ___KAHANA_MODULE___

#include "kahana.h"

typedef struct kModule_t kModule_t;

// load module for filename
apr_status_t kahanaModuleLoad( const char *filename, const char *label );
// module memory pool
apr_pool_t *kahanaModulePool( kModule_t *mod );
// number of loaded modules
int kahanaModuleNum( void );
// module list of method implemented
int kahanaModuleListForMethod( apr_pool_t *p, const char *mmethod, kModule_t *mods[] );

// return module context pointer
void **kahanaModuleContext( kModule_t *mod );
apr_status_t kahanaModuleLabelContext( const char *label, void **ctx );

apr_status_t kahanaModuleMethodPreload( int breakIfNoImp, const char *mmethod, ... );
apr_status_t kahanaModuleMethod( kModule_t *mod, const char *mmethod, apr_dso_handle_sym_t *sym );
typedef apr_status_t (*KMODULE_METHODINVOKE)( apr_pool_t *, void *, void **, void * );
apr_status_t kahanaModuleMethodInvoke( kModule_t *mod, const char *mmethod, KMODULE_METHODINVOKE callback, void *ctx );
apr_status_t kahanaModuleLabelMethodInvoke( const char *label, const char *mmethod, KMODULE_METHODINVOKE callback, void *ctx );

#endif
