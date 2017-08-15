/* mock implementation for pxfuriparser.h */
GPHDUri*
parseGPHDUri(const char* uri_str)
{
    check_expected(uri_str);
    return (GPHDUri	*) mock();
}

void
freeGPHDUri(GPHDUri* uri)
{
    check_expected(uri);
    mock();
}
