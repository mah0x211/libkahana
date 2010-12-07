/*
 *  kahana_image.h
 *
 */

#ifndef ___KAHANA_IMAGE___
#define ___KAHANA_IMAGE___

#include "kahana.h"
#include "Imlib2.h"

typedef enum {
	NOERR = IMLIB_LOAD_ERROR_NONE,
	FILE_DOES_NOT_EXIST = IMLIB_LOAD_ERROR_FILE_DOES_NOT_EXIST,
	FILE_IS_DIRECTORY = IMLIB_LOAD_ERROR_FILE_IS_DIRECTORY,
	PERMISSION_DENIED_TO_READ = IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_READ,
	NO_LOADER_FOR_FILE_FORMAT = IMLIB_LOAD_ERROR_NO_LOADER_FOR_FILE_FORMAT,
	PATH_TOO_LONG = IMLIB_LOAD_ERROR_PATH_TOO_LONG,
	PATH_COMPONENT_NON_EXISTANT = IMLIB_LOAD_ERROR_PATH_COMPONENT_NON_EXISTANT,
	PATH_COMPONENT_NOT_DIRECTORY = IMLIB_LOAD_ERROR_PATH_COMPONENT_NOT_DIRECTORY,
	PATH_POINTS_OUTSIDE_ADDRESS_SPACE = IMLIB_LOAD_ERROR_PATH_POINTS_OUTSIDE_ADDRESS_SPACE,
	TOO_MANY_SYMBOLIC_LINKS = IMLIB_LOAD_ERROR_TOO_MANY_SYMBOLIC_LINKS,
	OUT_OF_MEMORY = IMLIB_LOAD_ERROR_OUT_OF_MEMORY,
	OUT_OF_FILE_DESCRIPTORS = IMLIB_LOAD_ERROR_OUT_OF_FILE_DESCRIPTORS,
	PERMISSION_DENIED_TO_WRITE = IMLIB_LOAD_ERROR_PERMISSION_DENIED_TO_WRITE,
	OUT_OF_DISK_SPACE = IMLIB_LOAD_ERROR_OUT_OF_DISK_SPACE,
	UNKNOWN = IMLIB_LOAD_ERROR_UNKNOWN,
	FORMAT_UNACCEPTABLE = 1000
} kImageErrorType_e;

typedef enum {
	ALIGN_NONE,
	ALIGN_LEFT = 1,
	ALIGN_CENTER,
	ALIGN_RIGHT,
	
	ALIGN_TOP = 1,
	ALIGN_MIDDLE,
	ALIGN_BOTTOM
} kImageAlign_e;

typedef enum {
	RESIZE_DATUM_NONE = 0,
	RESIZE_DATUM_WIDTH,
	RESIZE_DATUM_HEIGHT,
} kImageResizeDatum_e;


typedef struct kImageContext_t kImageContext_t;

apr_status_t kahanaImgNewContext( kImageContext_t **newctx, apr_pool_t *p );
void kahanaImgDestroyContext( kImageContext_t *ctx );

const char *kahanaImgErrStr( kImageErrorType_e err );

unsigned int kahanaImgQuality( kImageContext_t *ctx );
void kahanaImgSetQuality( kImageContext_t *ctx, unsigned int quality );
unsigned int kahanaImgRawWidth( kImageContext_t *ctx );
unsigned int kahanaImgRawHeight( kImageContext_t *ctx );
const char *kahanaImgRawFmt( kImageContext_t *ctx );

kImageErrorType_e kahanaImgLoad( kImageContext_t *ctx, const char *src, ... );
void kahanaImgCrop( kImageContext_t *ctx, double aspect, kImageAlign_e holizontal, kImageAlign_e vertical );
void kahanaImgResize( kImageContext_t *ctx, double percentage, unsigned int width, unsigned int height, kImageAlign_e datum );
kImageErrorType_e kahanaImgSave( kImageContext_t *ctx, const char *dest );

#endif


