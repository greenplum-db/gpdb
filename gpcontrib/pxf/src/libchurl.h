/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef _GPHDFS_LIBCHURL_H_
#define _GPHDFS_LIBCHURL_H_

#include "pxfutils.h"

/*
 * CHunked cURL API
 * NOTES:
 * 1) Does not multi thread
 * 2) Does not talk IPv6
 */
typedef void *CHURL_HEADERS;
typedef void *CHURL_HANDLE;

/*
 * PUT example
 * -----------
 *
 * CHURL_HEADERS http_headers = churl_headers_init();
 * churl_headers_append(http_headers, "a", "b");
 * churl_headers_append(http_headers, "c", "d");
 *
 * CHURL_HANDLE churl = churl_init_upload("http://127.0.0.1:12345", http_headers);
 * while(have_stuff_to_write())
 * {
 *     churl_write(churl);
 * }
 *
 * churl_cleanup(churl);
 * churl_headers_cleanup(http_headers);
 *
 * GET example
 * -----------
 *
 * CHURL_HEADERS http_headers = churl_headers_init();
 * churl_headers_append(http_headers, "a", "b");
 * churl_headers_append(http_headers, "c", "d");
 *
 * CHURL_HANDLE churl = churl_init_download("http://127.0.0.1:12345", http_headers);
 *
 * char buf[64 * 1024];
 * size_t n = 0;
 * while ((n = churl_read(churl, buf, sizeof(buf))) != 0)
 * {
 *     do_something(buf, n);
 * }
 *
 * churl_cleanup(churl);
 * churl_headers_cleanup(http_headers);
 */

/*
 * Create a handle for adding headers
 */
CHURL_HEADERS churl_headers_init(void);

/*
 * Add a new header
 * Headers are added in the form 'key: value'
 */
void		churl_headers_append(CHURL_HEADERS headers, const char *key, const char *value);

/*
 * Override header with given 'key'.
 * If header doesn't exist, create new one (using churl_headers_append).
 * Headers are added in the form 'key: value'
 */
void		churl_headers_override(CHURL_HEADERS headers, const char *key, const char *value);

/*
 * Remove header with given 'key'.
 * has_value specifies if the header has a value or only a key.
 * If the header doesn't exist, do nothing.
 * If the header is the first one on the list,
 * point the headers list to the next element.
 */
void		churl_headers_remove(CHURL_HEADERS headers, const char *key, bool has_value);

/*
 * Cleanup handle for headers
 */
void		churl_headers_cleanup(CHURL_HEADERS headers);

/*
 * Start an upload to url
 * returns a handle to churl transfer
 */
CHURL_HANDLE churl_init_upload(const char *url, CHURL_HEADERS headers);

/*
 * Start a download to url
 * returns a handle to churl transfer
 */
CHURL_HANDLE churl_init_download(const char *url, CHURL_HEADERS headers);

/*
 * Restart a session to a new URL
 * This will use the same headers
 */
void		churl_download_restart(CHURL_HANDLE, const char *url, CHURL_HEADERS headers);

/*
 * Send buf of bufsize
 */
size_t		churl_write(CHURL_HANDLE handle, const char *buf, size_t bufsize);

/*
 * Receive up to max_size into buf
 */
size_t		churl_read(CHURL_HANDLE handle, char *buf, size_t max_size);

/*
 * Check connectivity by reading some bytes and checking response
 */
void		churl_read_check_connectivity(CHURL_HANDLE handle);

/*
 * Cleanup churl resources
 */
void		churl_cleanup(CHURL_HANDLE handle, bool after_error);

/*
 * Debug function - print the http headers
 */
void		print_http_headers(CHURL_HEADERS headers);

#define LocalhostIpV4Entry ":127.0.0.1"
#define LocalhostIpV4 "localhost"
#define REST_HEADER_JSON_RESPONSE "Accept: application/json"

#endif							/* _GPHDFS_LIBCHURL_H_ */
