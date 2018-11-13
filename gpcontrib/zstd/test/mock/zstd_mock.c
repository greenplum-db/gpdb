/* mock functions for zstd.h */
size_t
ZSTD_compressCCtx(ZSTD_CCtx * ctx, void * dst, size_t dstCapacity, const void * src, size_t srcSize, int compressionLevel)
{
	return (size_t)-ZSTD_error_dstSize_tooSmall;
}
