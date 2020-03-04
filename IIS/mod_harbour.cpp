#include "precomp.h"

extern "C" {
	typedef int ( * PHB_APACHE )( void * pRequestRec, void * pAPRPuts,
		const char * szFileName, const char * szArgs, const char * szMethod, const char * szUserIP,
		void * pHeadersIn, void * pHeadersOut,
		void * pHeadersInCount, void * pHeadersInKey, void * pHeadersInVal,
		void * pHeadersOutCount, void * pHeadersOutSet, void * pSetContentType,
		void * pApacheGetenv, void * pAPBody, long lAPRemaining );

   const char * ap_getenv( const char * szVarName, IHttpContext * pHttpContext );

	char * GetErrorMessage( DWORD dwLastError )
	{
		LPVOID lpMsgBuf;

		FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM,
			NULL,
			dwLastError,
			MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ), // Default language
			( LPTSTR ) &lpMsgBuf,
			0,
			NULL );

		return ( char * ) lpMsgBuf;
		LocalFree( lpMsgBuf );
	}

	int ap_rputs( const char * szText, IHttpContext * pHttpContext )
	{
		HTTP_DATA_CHUNK dataChunk;
		PCSTR pszText = ( PCSTR ) pHttpContext->AllocateRequestMemory( strlen( szText ) + 1 );
		IHttpResponse * pHttpResponse = pHttpContext->GetResponse();

		if( pszText )
		{
			strcpy( ( char * ) pszText, szText );

			dataChunk.DataChunkType = HttpDataChunkFromMemory;
			dataChunk.FromMemory.pBuffer = ( PVOID ) pszText;
			dataChunk.FromMemory.BufferLength = ( ULONG ) strlen( pszText );

			return pHttpResponse->WriteEntityChunkByReference( &dataChunk, -1 );
		}
		else
			return 0;
	}


	int ap_headers_out_count( IHttpContext * pHttpContext )
	{
	   return HttpHeaderMaximum;
	}

	const char * ap_headers_in_key( int iKey, IHttpContext * pHttpContext )
	{
      IHttpResponse * pHttpResponse = pHttpContext->GetResponse();
      USHORT cchKey = 0;

      pHttpResponse->GetHeader( ( IN HTTP_HEADER_ID ) iKey, &cchKey );

      if( cchKey > 0 )
         return pHttpResponse->GetHeader( ( IN HTTP_HEADER_ID ) iKey, &cchKey );
      else
	      return "";
	}

	const char * ap_headers_in_val( int iKey, IHttpContext * pHttpContext )
	{
      IHttpResponse * pHttpResponse = pHttpContext->GetResponse();
      USHORT cchKey = 0;

      pHttpResponse->GetHeader( ( IN HTTP_HEADER_ID ) iKey, &cchKey );

      if( cchKey > 0 )
         return pHttpResponse->GetHeader( ( IN HTTP_HEADER_ID ) iKey, &cchKey );
      else
         return "";
   }

	int ap_headers_in_count( IHttpContext * pHttpContext )
	{
	   const char * szHeaders = ap_getenv( "ALL_HTTP", pHttpContext );
      int iCount = 0; char * pPos = ( char * ) szHeaders;

      while( pPos = strstr( pPos, "HTTP_" ) )
      { 
         pPos += 5;
         iCount++; 
      }
              
      return iCount;
	}

	void ap_headers_out_set( const char * szKey, const char * szValue, IHttpContext * pHttpContext )
	{
	   IHttpResponse * pHttpResponse = pHttpContext->GetResponse();

	   pHttpResponse->SetHeader( szKey, szValue, strlen( szValue ), true );	
	}

	void ap_set_contenttype( const char * szContentType, IHttpContext * pHttpContext )
	{
	   IHttpResponse * pHttpResponse = pHttpContext->GetResponse();

	   pHttpResponse->SetHeader( "Content-Type", szContentType, strlen( szContentType ), true );
	}

	const char * ap_getenv( const char * szVarName, IHttpContext * pHttpContext )
	{
	   PCSTR rawBuffer = NULL;
	   DWORD rawLength = 0;

	   pHttpContext->GetServerVariable( szVarName, &rawBuffer, &rawLength );

	   return rawBuffer;
	}

	const char * ap_args( IHttpContext * pHttpContext )
	{
	   return ap_getenv( "QUERY_STRING", pHttpContext );
	}

	const char * ap_body( IHttpContext * pHttpContext )
	{
      DWORD bytesRead = 0;
      int totalBytesRead = 0;
      int bytesToRead = atoi( ap_getenv( "CONTENT_LENGTH", pHttpContext ) ), iSize;
      IHttpRequest * request = pHttpContext->GetRequest();
      char * buffer = ( char * ) pHttpContext->AllocateRequestMemory( bytesToRead );
      BOOL bCompletionPending = false;

      iSize = bytesToRead;
       
	   if( buffer )
	   {
	      while( bytesToRead > 0 )
	      {
	         request->ReadEntityBody( ( char * ) ( buffer + bytesRead ), bytesToRead, false, &bytesRead, &bCompletionPending );

      		if( ! bytesRead )
		         break;

		      bytesToRead -= bytesRead;
	      }

         * ( buffer + iSize ) = 0;
	   }

	   return buffer;	
   }
}

static long lAPRemaining = 0;

REQUEST_NOTIFICATION_STATUS CMyHttpModule::OnAcquireRequestState( IN IHttpContext * pHttpContext,
									                  							IN OUT IHttpEventProvider * pProvider )
{
	const char * szPathInfo;

	szPathInfo = ap_getenv( "PATH_INFO", pHttpContext );

	if( strstr( szPathInfo, ".prg" ) || strstr( szPathInfo, ".hrb" ) )
	{
		HMODULE lib_harbour = LoadLibrary( "c:\\Windows\\System32\\inetsrv\\libharbour.dll" );

		ap_set_contenttype( "text/html", pHttpContext );

		if( lib_harbour == NULL )
		{
			char * szErrorMessage = GetErrorMessage( GetLastError() );
			ap_rputs( "c:\\Windows\\System32\\inetsrv\\libharbour.dll - ", pHttpContext );
			ap_rputs( szErrorMessage, pHttpContext );
			LocalFree( ( void * ) szErrorMessage );
		}
		else
		{
			PHB_APACHE _hb_apache = ( PHB_APACHE ) GetProcAddress( lib_harbour, "hb_apache" );
			char szPath[ 512 ];

			strcpy( szPath, ap_getenv( "APPL_PHYSICAL_PATH", pHttpContext ) );
			strcat( szPath, szPathInfo + 1 );
         while( strchr( szPath, '\\' ) )
            * strchr( szPath, '\\' ) = '/';

			if( _hb_apache != NULL )
			{
 				_hb_apache( pHttpContext, ap_rputs, szPath, ap_args( pHttpContext ), 
					        ap_getenv( "REQUEST_METHOD", pHttpContext ), ap_getenv( "REMOTE_ADDR", pHttpContext ),
							  NULL, NULL,
							  ( void * ) ap_headers_in_count, ( void * ) ap_headers_in_key, ( void * ) ap_headers_in_val,
							  ( void * ) ap_headers_out_count, ( void * ) ap_headers_out_set, ( void * ) ap_set_contenttype,
							  ( void * ) ap_getenv, ( void * ) ap_body, lAPRemaining );
 			}
			
			if( _hb_apache == NULL )
				ap_rputs( "can't find hb_apache()", pHttpContext );
		}

		if( lib_harbour )
			FreeLibrary( lib_harbour );
		
		return RQ_NOTIFICATION_FINISH_REQUEST;
	}
	else
       return RQ_NOTIFICATION_CONTINUE;
}
