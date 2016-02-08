/*-------------------------------------------------------------------------
 *
 * strlcat.h
 *	  Header for strlcat function
 *
 *-------------------------------------------------------------------------
 */

#if !HAVE_DECL_STRLCAT
extern size_t strlcat(char *dst, const char *src, size_t siz);
#endif
