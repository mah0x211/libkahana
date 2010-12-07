#include "kahana_module.h"
#include "kahana_private.h"

struct kModuleLoader_t{
	apr_pool_t *p;
	apr_hash_t *mods;
	int nmods;
};

struct kModule_t{
	apr_pool_t *p;
	kModuleLoader_t *loader;
	const char *label;
	const char *filename;
	apr_dso_handle_t *handle;
	apr_hash_t *methods;
	void *ctx;
};

typedef struct {
	apr_pool_t *p;
	kModule_t *mod;
	const char *name;
	apr_dso_handle_sym_t sym;
} kMethod_t;

#pragma mark Static
static kModuleLoader_t *GetModuleLoader( void ){
	Kahana_t *kahana = _kahanaGlobal();
	return kahana->loader;
}

static apr_status_t cbModuleCleanupMod( void *ctx )
{
	kModule_t *mod = (kModule_t*)ctx;
	apr_status_t rc;
	
	if( mod )
	{
		kModuleLoader_t *loader = GetModuleLoader();
		
		apr_hash_set( loader->mods, mod->filename, APR_HASH_KEY_STRING, NULL );
		if( mod->handle && ( rc = apr_dso_unload( mod->handle ) ) ){
			kahanaLogPut( NULL, NULL, "failed to apr_dso_unload() reason %s", kahanaLogErr2Str( ETYPE_APR, rc ) );
		}
	}
	
	return APR_SUCCESS;
}

static apr_status_t _kahanaModuleMethodLoad( kModule_t *mod, const char *mmethod )
{
	apr_dso_handle_sym_t sym = NULL;
	apr_status_t rc = apr_dso_sym( &sym, mod->handle, mmethod );
	kMethod_t *method = NULL;
	apr_pool_t *p = NULL;
	
	if( rc == APR_SUCCESS && 
		( rc = kahanaMalloc( mod->p, sizeof( kMethod_t ), (void**)&method, &p ) ) == APR_SUCCESS )
	{
		method->p = p;
		method->mod = mod;
		method->name = apr_pstrdup( mod->p, mmethod );
		method->sym = sym;
		apr_hash_set( mod->methods, method->name, APR_HASH_KEY_STRING, (void*)method );
	}
	
	return rc;
}


#pragma mark Method
apr_status_t kahanaModuleLabelMethodInvoke( const char *label, const char *mmethod, KMODULE_METHODINVOKE callback, void *ctx )
{
	kModuleLoader_t *loader = GetModuleLoader();
	kModule_t *mod = (kModule_t*)apr_hash_get( loader->mods, label, APR_HASH_KEY_STRING );
	apr_status_t rc = APR_SUCCESS;
	
	if( mod ){
		rc = kahanaModuleMethodInvoke( mod, mmethod, callback, ctx );
	}
	else {
		rc = APR_ENOENT;
	}
	
	return rc;
}

/*
retval:
	APR_SUCCESS = method completed
	APR_EINPROGRESS = method in-progress
	APR_ESYMNOTFOUND = method not found
*/
apr_status_t kahanaModuleMethodInvoke( kModule_t *mod, const char *mmethod, KMODULE_METHODINVOKE callback, void *ctx )
{
	apr_dso_handle_sym_t sym = NULL;
	apr_status_t rc = kahanaModuleMethod( mod, mmethod, &sym );
	
	if( rc == APR_SUCCESS ){
		rc = callback( mod->p, sym, &mod->ctx, ctx );
	}
	
	return rc;
}

apr_status_t kahanaModuleMethod( kModule_t *mod, const char *mmethod, apr_dso_handle_sym_t *sym )
{
	apr_status_t rc = APR_SUCCESS;
	kMethod_t *method = (kMethod_t*)apr_hash_get( mod->methods, mmethod, APR_HASH_KEY_STRING );
	
	// try to load method if method not found
	if( !method &&
		( rc = _kahanaModuleMethodLoad( mod, mmethod ) ) == APR_SUCCESS )
	{
		method = (kMethod_t*)apr_hash_get( mod->methods, mmethod, APR_HASH_KEY_STRING );
	
		/*
		if( rc != APR_ESYMNOTFOUND ){
			char buf[HUGE_STRING_LEN];
			fprintf( stderr, "failed to kahanaModuleMethod() module:%s method:%s reasons %s\n", mod->label, mmethod, apr_dso_error( mod->handle, buf, HUGE_STRING_LEN ) );
			//kahanaLogPut( NULL, NULL, "failed to kahanaModuleMethod() module %s reasons %s", mod->label, apr_dso_error( mod->handle, buf, HUGE_STRING_LEN ) );
		}
		*/
	}
	
	if( rc == APR_SUCCESS ){
		*sym = method->sym;
	}
	return rc;
}

/*
retval:
	APR_SUCCESS
	APR_ENOENT = module not found
	APR_ESYMNOTFOUND = method not found
*/
apr_status_t kahanaModuleMethodPreload( int breakIfNoImp, const char *mmethod, ... )
{
	kModuleLoader_t *loader = GetModuleLoader();
	apr_status_t rc = APR_SUCCESS;
	char buf[HUGE_STRING_LEN];
	va_list args;
	kModule_t *mod;
	kMethod_t *method;
	const char *mlabel;
	
	va_start( args, mmethod );
	while( ( mlabel = va_arg( args, const char* ) ) )
	{
		if( ( mod = (kModule_t*)apr_hash_get( loader->mods, mlabel, APR_HASH_KEY_STRING ) ) )
		{
			if( !( method = (kMethod_t*)apr_hash_get( mod->methods, mmethod, APR_HASH_KEY_STRING ) ) &&
				( rc = _kahanaModuleMethodLoad( mod, mmethod ) ) )
			{
				kahanaLogPut( NULL, NULL, "failed to kahanaModuleMethodPreload() module %s reasons %s", mlabel, apr_dso_error( mod->handle, buf, HUGE_STRING_LEN ) );
				if( breakIfNoImp ){
					break;
				}
				else {
					rc = APR_SUCCESS;
				}
			}
		}
		else {
			rc = APR_ENOENT;
			kahanaLogPut( NULL, NULL, "failed to kahanaModuleMethodPreload() reason module %s not found", mlabel );
			break;
		}
	}
	va_end( args );

	return rc;
}


#pragma mark Module Context
/*
retval:
	APR_SUCCESS
	APR_ENOENT = module not found
*/
apr_status_t kahanaModuleLabelContext( const char *label, void **ctx )
{
	apr_status_t rc = APR_SUCCESS;
	kModuleLoader_t *loader = GetModuleLoader();
	kModule_t *mod = (kModule_t*)apr_hash_get( loader->mods, label, APR_HASH_KEY_STRING );
	
	if( mod ){
		*ctx = mod->ctx;
	}
	else {
		rc = APR_ENOENT;
	}
	
	return rc;
}

void **kahanaModuleContext( kModule_t *mod ){
	return &mod->ctx;
}

#pragma mark Module
int kahanaModuleList( apr_pool_t *p, kModule_t *mods[] )
{
	kModuleLoader_t *loader = GetModuleLoader();
	apr_hash_index_t *idx;
	int nmod = 0;
	int i = 0;
	
	for( idx = apr_hash_first( p, loader->mods ); idx; idx = apr_hash_next( idx ) )
	{
		kModule_t *mod = NULL;
		
		apr_hash_this( idx, NULL, NULL, (void**)&mod );
		if( mod ){
			mods[i++] = mod;
			nmod++;
		}
	}
	
	return nmod;
}

int kahanaModuleListForMethod( apr_pool_t *p, const char *mmethod, kModule_t *mods[] )
{
	int nmod = 0;
	
	if( mmethod )
	{
		kModuleLoader_t *loader = GetModuleLoader();
		apr_hash_index_t *idx;
		int i = 0;
		
		for( idx = apr_hash_first( p, loader->mods ); idx; idx = apr_hash_next( idx ) )
		{
			kModule_t *mod = NULL;
			kMethod_t *method;
			
			apr_hash_this( idx, NULL, NULL, (void**)&mod );
			if( mod && ( method = (kMethod_t*)apr_hash_get( mod->methods, mmethod, APR_HASH_KEY_STRING ) ) ){
				mods[i++] = mod;
				nmod++;
			}
		}
	}
	
	return nmod;
}

/*
retval:
	int = number of loaded modules
*/
int kahanaModuleNum( void )
{
	kModuleLoader_t *loader = GetModuleLoader();
	return loader->nmods;
}

apr_pool_t *kahanaModulePool( kModule_t *mod ){
	return mod->p;
}

/*
retval:
	APR_SUCCESS
	APR_ENOENT = module not found
*/
apr_status_t kahanaModuleLoad( const char *filename, const char *label )
{
	apr_status_t rc = APR_SUCCESS;
	kModuleLoader_t *loader = GetModuleLoader();
	kModule_t *mod = (kModule_t*)apr_hash_get( loader->mods, label, APR_HASH_KEY_STRING );
	apr_pool_t *p = NULL;
	
	if( !mod && ( rc = kahanaMalloc( loader->p, sizeof( kModule_t ), (void**)&mod, &p ) ) == APR_SUCCESS )
	{
		mod->p = p;
		mod->filename = (const char*)apr_pstrdup( p, filename );
		mod->label = ( label ) ? (const char*)apr_pstrdup( p, label ) : mod->filename;
		mod->handle = NULL;
		mod->methods = apr_hash_make( p );
		
		// load element dso
		if( ( rc = apr_dso_load( &mod->handle, filename, p ) ) ){
			apr_pool_destroy( p );
		}
		// load methods
		else{
			apr_hash_set( loader->mods, mod->label, APR_HASH_KEY_STRING, (void*)mod );
			loader->nmods++;
			apr_pool_cleanup_register( p, (void*)mod, cbModuleCleanupMod, apr_pool_cleanup_null );
		}
	}
	
	return rc;
}

#pragma mark Initialize
apr_status_t _kahanaModuleInit( Kahana_t *kahana )
{
	apr_pool_t *p = NULL;
	apr_status_t rc = kahanaMalloc( kahana->p, sizeof( kModuleLoader_t ), (void**)&kahana->loader, &p );
	
	if( rc == APR_SUCCESS ){
		kahana->loader->p = p;
		kahana->loader->mods = apr_hash_make( p );
		kahana->loader->nmods = 0;
	}
	
	return rc;
}

void _kahanaModuleCleanup( void )
{
	kModuleLoader_t *loader = GetModuleLoader();
	kModule_t *mods[loader->nmods];
	int nmods = kahanaModuleList( loader->p, mods );
	int i;
	
	for( i = 0; i < nmods; i++ ){
		apr_pool_destroy( mods[i]->p );
	}
	apr_pool_destroy( loader->p );
}