/* 
**
**  Modify : 8/20/2006
**  (C) Masatoshi Teruya
**
*/

#include "kahana_image.h"

typedef struct {
	int w;
	int h;
	double aspect;
} ImageRect;

struct kImageContext_t{
	apr_pool_t *p;
	Imlib_Image img;
	int attached;
	const char *format;
	const char *src;
	
	unsigned int quality;
	int x;
	int y;
	ImageRect size;
	int cropped;
	ImageRect crop;
	int resized;
	ImageRect resize;
};


static apr_status_t ImgCleanupContext( void *data )
{
	if( imlib_context_get_image() )
	{
		if( imlib_get_cache_size() ){
			imlib_free_image_and_decache();
		}
		else {
			imlib_free_image();
		}
	}
	
	return APR_SUCCESS;
}

static const char *ImlibLoadErrStr( kImageErrorType_e err )
{
	const char *errstr = NULL;
	
	switch( err )
	{
		case NOERR:
			errstr = "NONE";
		break;

		case FILE_DOES_NOT_EXIST:
			errstr = "FILE_DOES_NOT_EXIST";
		break;

		case FILE_IS_DIRECTORY:
			errstr = "FILE_IS_DIRECTORY";
		break;

		case PERMISSION_DENIED_TO_READ:
			errstr = "PERMISSION_DENIED_TO_READ";
		break;

		case NO_LOADER_FOR_FILE_FORMAT:
			errstr = "NO_LOADER_FOR_FILE_FORMAT";
		break;

		case PATH_TOO_LONG:
			errstr = "PATH_TOO_LONG";
		break;

		case PATH_COMPONENT_NON_EXISTANT:
			errstr = "PATH_COMPONENT_NON_EXISTANT";
		break;

		case PATH_COMPONENT_NOT_DIRECTORY:
			errstr = "PATH_COMPONENT_NOT_DIRECTORY";
		break;

		case PATH_POINTS_OUTSIDE_ADDRESS_SPACE:
			errstr = "PATH_POINTS_OUTSIDE_ADDRESS_SPACE";
		break;

		case TOO_MANY_SYMBOLIC_LINKS:
			errstr = "TOO_MANY_SYMBOLIC_LINKS";
		break;

		case OUT_OF_MEMORY:
			errstr = "OUT_OF_MEMORY";
		break;

		case OUT_OF_FILE_DESCRIPTORS:
			errstr = "OUT_OF_FILE_DESCRIPTORS";
		break;

		case PERMISSION_DENIED_TO_WRITE:
			errstr = "PERMISSION_DENIED_TO_WRITE";
		break;

		case OUT_OF_DISK_SPACE:
			errstr = "OUT_OF_DISK_SPACE";
		break;
		
		case FORMAT_UNACCEPTABLE:
			errstr = "UNACCEPTABLE_IMAGE_FORMAT";
		break;
		
		case UNKNOWN:
			errstr = "UNKNOWN";
		break;
	}
	
	return errstr;
}

apr_status_t kahanaImgNewContext( kImageContext_t **newctx, apr_pool_t *p )
{
	apr_status_t rc;
	kImageContext_t *ctx = NULL;
	apr_pool_t *sp = NULL;
	
	if( ( rc = kahanaMalloc( p, sizeof( kImageContext_t ), (void**)&ctx, &sp ) ) == APR_SUCCESS )
	{
		ctx->p = sp;
		ctx->img = NULL;
		ctx->attached = 0;
		ctx->format = NULL;
		ctx->src = NULL;
		ctx->quality = 100;
		ctx->x = ctx->y = 0;
		ctx->size.w = ctx->crop.w = ctx->resize.w = 0;
		ctx->size.h = ctx->crop.h = ctx->resize.h = 0;
		ctx->size.aspect = ctx->crop.aspect = 1;
		apr_pool_cleanup_register( sp, (void*)ctx, ImgCleanupContext, apr_pool_cleanup_null );
		*newctx = ctx;
	}
	
	return rc;
}

void kahanaImgDestroyContext( kImageContext_t *ctx )
{
	apr_pool_destroy( ctx->p );
}

const char *kahanaImgErrStr( kImageErrorType_e err )
{
	return ImlibLoadErrStr( err );
}

unsigned int kahanaImgQuality( kImageContext_t *ctx )
{
	return ctx->quality;
}
void kahanaImgSetQuality( kImageContext_t *ctx, unsigned int quality )
{
	ctx->quality = ( quality ) ? quality : ctx->quality;
}

unsigned int kahanaImgRawWidth( kImageContext_t *ctx ){
	return ctx->size.w;
}
unsigned int kahanaImgRawHeight( kImageContext_t *ctx ){
	return ctx->size.h;
}
const char *kahanaImgRawFmt( kImageContext_t *ctx ){
	return ctx->format;
}


kImageErrorType_e kahanaImgLoad( kImageContext_t *ctx, const char *src, ... )
{
	ImlibLoadError imerr = NOERR;
	const char *accept;
	va_list args;
	
	ctx->img = imlib_load_image_with_error_return( src, &imerr );
	if( imerr ){
		return imerr;
	}
	
	// check image format
	imlib_context_set_image( ctx->img );
	ctx->attached = 1;
	ctx->format = imlib_image_format();
	va_start( args, src );
	while( ( accept = va_arg( args, char* ) ) )
	{
		if( !kahanaStrCmp( ctx->format, accept ) ){
			imerr = FORMAT_UNACCEPTABLE;
			break;
		}
	}
	va_end( args );
	
	if( imerr == NOERR ){
		ctx->src = (const char*)apr_pstrdup( ctx->p, src );
		ctx->quality = 100;
		ctx->cropped = ctx->resized = 0;
		ctx->size.w = ctx->crop.w = ctx->resize.w = imlib_image_get_width();
		ctx->size.h = ctx->crop.h = ctx->resize.h = imlib_image_get_height();
		ctx->size.aspect = ctx->crop.aspect = (double)ctx->size.w/(double)ctx->size.h;
	}
	
	return imerr;
}

void kahanaImgCrop( kImageContext_t *ctx, double aspect, kImageAlign_e holizontal, kImageAlign_e vertical )
{
	if( aspect )
	{
		ctx->cropped = 1;
		if( ctx->size.aspect > aspect ){
			ctx->crop.w = ctx->size.h * aspect;
			ctx->crop.h = ctx->size.h;
			switch( holizontal )
			{
				case ALIGN_LEFT:
					ctx->x = 0;
				break;
				
				case ALIGN_CENTER:
					ctx->x = ( ctx->size.w - ctx->crop.w ) / 2;
				break;
				
				case ALIGN_RIGHT:
					ctx->x = ctx->size.w - ctx->crop.w;
				break;
				
				case ALIGN_NONE:
				break;
			}
		}
		else if( ctx->size.aspect < aspect ){
			ctx->crop.h = ctx->size.w / aspect;
			ctx->crop.w = ctx->size.w;
			switch( vertical )
			{
				case ALIGN_TOP:
					ctx->y = 0;
				break;
				
				case ALIGN_MIDDLE:
					ctx->y = ( ctx->size.h - ctx->crop.h ) / 2;
				break;
				
				case ALIGN_BOTTOM:
					ctx->y = ctx->size.h - ctx->crop.h;
				break;
				
				case ALIGN_NONE:
				break;
			}
		}
		else {
			ctx->cropped = 0;
		}
		
		if( ctx->cropped ){
			ctx->crop.aspect = (double)ctx->crop.w/(double)ctx->crop.h;
		}
	}
}

void kahanaImgResize( kImageContext_t *ctx, double percentage, unsigned int width, unsigned int height, kImageAlign_e datum )
{
	double w = ctx->size.w;
	double h = ctx->size.h;
	double aspect = ctx->size.aspect;
	
	// if cropped
	if( ctx->cropped ){
		w = ctx->crop.w;
		h = ctx->crop.h;
		aspect = ctx->crop.aspect;
	}
	
	if( percentage > 0 ){
		ctx->resize.w = ( w / 100 ) * percentage;
		ctx->resize.h = ( h / 100 ) * percentage;
		ctx->resized = 1;
	}
	else if( w != width || h != height )
	{
		if( datum == RESIZE_DATUM_NONE ){
			ctx->resize.w = width;
			ctx->resize.h = height;
			ctx->resized = 1;
		}
		else
		{
			if( datum == RESIZE_DATUM_WIDTH && width ){
				ctx->resize.w = width;
				ctx->resize.h = width / aspect;
				ctx->resized = 1;
			}
			else if( datum == RESIZE_DATUM_HEIGHT && height ){
				ctx->resize.w = height * aspect;
				ctx->resize.h = height;
				ctx->resized = 1;
			}
		}
	}
}

kImageErrorType_e kahanaImgSave( kImageContext_t *ctx, const char *dest )
{
	ImlibLoadError imerr = NOERR;
	Imlib_Image img;
	Imlib_Image clone;
	
	imlib_context_set_image( ctx->img );
	// clone
	clone = imlib_clone_image();
	
	// crop
	if( ctx->cropped ){
		img = imlib_create_cropped_image( ctx->x, ctx->y, ctx->crop.w, ctx->crop.h );
		imlib_free_image_and_decache();
		ctx->img = img;
		imlib_context_set_image( ctx->img );
	}
	// resize
	if( ctx->resized ){
		if( ctx->cropped ){
			img = imlib_create_cropped_scaled_image( 0, 0, ctx->crop.w, ctx->crop.h, ctx->resize.w, ctx->resize.h );
		}
		else{
			img = imlib_create_cropped_scaled_image( 0, 0, ctx->size.w, ctx->size.h, ctx->resize.w, ctx->resize.h );
		}
		imlib_free_image_and_decache();
		ctx->img = img;
		imlib_context_set_image( ctx->img );
	}
	// quality
	imlib_image_attach_data_value( "quality", NULL, ctx->quality, NULL );
	imlib_save_image_with_error_return( dest, &imerr );
	imlib_free_image_and_decache();
	
	ctx->img = clone;
	imlib_context_set_image( ctx->img );
	
	return imerr;
}
