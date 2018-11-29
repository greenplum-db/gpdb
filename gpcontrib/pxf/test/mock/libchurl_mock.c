/* mock functions for libchurl.h */
CHURL_HEADERS
churl_headers_init(void)
{
    return (CHURL_HEADERS) mock();
}

void
churl_headers_append(CHURL_HEADERS headers, const char* key, const char* value)
{
    check_expected(headers);
    check_expected(key);
    check_expected(value);
    mock();
}

void
churl_headers_override(CHURL_HEADERS headers, const char* key, const char* value)
{
    check_expected(headers);
    check_expected(key);
    check_expected(value);
    mock();
}

void
churl_headers_remove(CHURL_HEADERS headers, const char* key, bool has_value)
{
    check_expected(headers);
    check_expected(key);
    check_expected(has_value);
    mock();
}

void
churl_headers_cleanup(CHURL_HEADERS headers)
{
    check_expected(headers);
    mock();
}

CHURL_HANDLE
churl_init_upload(const char* url, CHURL_HEADERS headers)
{
    check_expected(url);
    check_expected(headers);
    return (CHURL_HANDLE) mock();
}

CHURL_HANDLE
churl_init_download(const char* url, CHURL_HEADERS headers)
{
    check_expected(url);
    check_expected(headers);
    return (CHURL_HANDLE) mock();
}

void
churl_download_restart(CHURL_HANDLE handle, const char* url, CHURL_HEADERS headers)
{
    check_expected(handle);
    check_expected(url);
    check_expected(headers);
    mock();
}

size_t
churl_write(CHURL_HANDLE handle, const char* buf, size_t bufsize)
{
    check_expected(handle);
    check_expected(buf);
    check_expected(bufsize);
    return (size_t) mock();
}

size_t
churl_read(CHURL_HANDLE handle, char* buf, size_t max_size)
{
    check_expected(handle);
    check_expected(buf);
    check_expected(max_size);
    return (size_t) mock();
}

void
churl_read_check_connectivity(CHURL_HANDLE handle)
{
    check_expected(handle);
    mock();
}

void
churl_cleanup(CHURL_HANDLE handle, bool after_error)
{
    check_expected(handle);
    check_expected(after_error);
    mock();
}

void
print_http_headers(CHURL_HEADERS headers)
{
    check_expected(headers);
    mock();
}
