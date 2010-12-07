#ifndef ___KAHANA_PRIVATE___
#define ___KAHANA_PRIVATE___

typedef struct kIOCache_t kIOCache_t;
typedef struct kModuleLoader_t kModuleLoader_t;
typedef struct kJapaneseCharactorMap_t kJapaneseCharactorMap_t;
typedef struct kDb_t kDb_t;

typedef struct {
	apr_pool_t *p;
	bool debug;
	apr_hash_t *logs;
	kIOCache_t *iocache;
	kModuleLoader_t *loader;
	kJapaneseCharactorMap_t *jaMap;
//	kDb_t *database;
} Kahana_t;

Kahana_t *_kahanaGlobal( void );
apr_status_t _kahanaCurlWrapInit( void );
void _kahanaCurlWrapCleanup( void );
apr_status_t _kahanaStrInit( void );
void _kahanaStrCleanup( void );
apr_status_t _kahanaModuleInit( Kahana_t *kahana );
void _kahanaModuleCleanup( void );
apr_status_t _kahanaIOInit( Kahana_t *kahana );
//apr_status_t _kahanaDbInit( Kahana_t *kahana );

#endif
