#ifndef	__CURL_HELPER_H__
#define	__CURL_HELPER_H__

#include <curl/curl.h>

extern int curl_fetch_url(const char *url, char *contents, int size);
extern int curl_request_url(const char *url, const char *header, const char *request, char *contents, int size);

#endif
