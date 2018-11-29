/* mock functions for curl.h */
CURLcode
curl_easy_setopt(CURL* curl, CURLoption option, ...)
{
    check_expected(curl);
    check_expected(option);
    optional_assignment(curl);
    return (CURLcode) mock();
}

CURL*
curl_easy_init(void)
{
    return (CURL*) mock();
}

struct curl_slist*
curl_slist_append(struct curl_slist* list, const char* string)
{
    check_expected(list);
    check_expected(string);
    return (struct curl_slist*) mock();
}

CURLM* curl_multi_init(void)
{
    return (CURLM*) mock();
}

CURLMcode
curl_multi_add_handle(CURLM* multi_handle, CURL* curl_handle)
{
    check_expected(multi_handle);
    check_expected(curl_handle);
    return (CURLMcode) mock();
}

CURLMcode
curl_multi_perform(CURLM* multi_handle, int* running_handles)
{
    check_expected(multi_handle);
    check_expected(running_handles);
    optional_assignment(running_handles);
    return (CURLMcode) mock();
}

CURLMcode
curl_multi_fdset(CURLM* multi_handle, fd_set* read_fd_set, fd_set* write_fd_set, fd_set* exc_fd_set, int* max_fd)
{
    check_expected(multi_handle);
    check_expected(read_fd_set);
    check_expected(write_fd_set);
    check_expected(exc_fd_set);
    check_expected(max_fd);
    optional_assignment(read_fd_set);
    optional_assignment(write_fd_set);
    optional_assignment(exc_fd_set);
    optional_assignment(max_fd);
    return (CURLMcode) mock();
}

int
select(int nfds, fd_set* read_fds, fd_set* write_fds, fd_set* except_fds, struct timeval* timeout)
{
    check_expected(nfds);
    check_expected(read_fds);
    check_expected(write_fds);
    check_expected(except_fds);
    check_expected(timeout);
    return (int) mock();
}
