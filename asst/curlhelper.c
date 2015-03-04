#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <curlhelper.h>
#include <mischelper.h>

struct stringcontext {
	char *data;
	int ndata, sdata;
};

static size_t curl_handle_write(char *buffer, size_t size, size_t nmemb, void *userdata)
{
	struct stringcontext *context = (struct stringcontext *)userdata;

	ARRAY_APPEND_BUFFER(context->data, context->sdata, context->ndata, buffer, size * nmemb);

	return size * nmemb;
}

int curl_fetch_url(const char *url, char *contents, int size)
{
	struct stringcontext context;
	CURL *curl;
	CURLcode res;
	int length = 0;

	context.data = (char *)malloc(4096);
	if (context.data == NULL)
		return -1;
	memset(context.data, 0, 4096);
	context.ndata = 0;
	context.sdata = 4096;

	curl = curl_easy_init();
	if (curl != NULL) {
		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_handle_write);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);

		res = curl_easy_perform(curl);
		if (res == CURLE_OK) {
			strncpy(contents, context.data, size);

			length = context.ndata;
		}

		curl_easy_cleanup(curl);
	}

	free(context.data);

	return length;
}

int curl_request_url(const char *url, const char *header, const char *request, char *contents, int size)
{
	struct stringcontext context;
	struct curl_slist *clist = NULL;
	CURL *curl;
	CURLcode res;
	int length = 0;

	context.data = (char *)malloc(4096);
	if (context.data == NULL)
		return -1;
	memset(context.data, 0, 4096);
	context.ndata = 0;
	context.sdata = 4096;

	curl = curl_easy_init();
	if (curl != NULL) {
		clist = curl_slist_append(clist, header);

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, clist);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_handle_write);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, request);
		curl_easy_setopt(curl, CURLOPT_POSTFIELDS, contents);

		res = curl_easy_perform(curl);
		if (res == CURLE_OK) {
			strncpy(contents, context.data, size);

			length = context.ndata;
		}

		curl_easy_cleanup(curl);

		curl_slist_free_all(clist);
	}

	free(context.data);

	return length;
}
