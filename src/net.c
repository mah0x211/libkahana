/*
	printLog( NULL, "_SC_NPROCESSORS_ONLN: %d", sysconf( _SC_NPROCESSORS_ONLN ) );
	printLog( NULL, "_SC_CHILD_MAX: %d", sysconf( _SC_CHILD_MAX ) );
	printLog( NULL, "_SC_OPEN_MAX: %d", sysconf( _SC_OPEN_MAX ) );
	printLog( NULL, "_SC_LINE_MAX: %d", sysconf( _SC_LINE_MAX ) );
*/

#include "kahana_net.h"

apr_status_t kahanaNetCIDR( apr_pool_t *p, kCIDR_t *cidr_def, const char *cidr )
{
	apr_status_t rc = APR_SUCCESS;
	size_t len = strlen( cidr );
	char tmp[len];
	char *ptr = NULL;
	
	memmove( tmp, cidr, len );
	tmp[len] = '\0';
	if( !( ptr = strchr( tmp, '/' ) ) ){
		rc = APR_EINVAL;
		kahanaLogPut( NULL, NULL, "invalid CIDR format" );
	}
	else
	{
		struct in_addr mask = {0};
		
		*ptr++ = '\0';
		if( strchr( ptr, '.' ) ){
			mask.s_addr = inet_addr( ptr );
		}
		else
		{
			mask.s_addr = atoi( ptr );
			if( mask.s_addr > 32 || mask.s_addr <= 0 ){
				rc = APR_EINVAL;
				kahanaLogPut( NULL, NULL, "invalid CIDR mask range[1-32]" );
			}
			else
			{
				/*
				11111111 11111111 11111111 11111111 << ( 32bit - mask )
				if mask = 2
				11111111 11111111 11111111 11111100
				if mask = 3
				11111111 11111111 11111111 11111000
				*/
				mask.s_addr = 0xFFFFFFFFUL << ( 32 - mask.s_addr );
				mask.s_addr = htonl( mask.s_addr );
			}
		}
		
		if( rc == APR_SUCCESS ){
			cidr_def->mask = mask.s_addr;
			cidr_def->from = inet_addr( tmp );
			cidr_def->to = cidr_def->from + ( cidr_def->mask ^ 0xFFFFFFFFUL );
		}
	}
	
	return rc;
}

