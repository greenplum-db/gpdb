/*-------------------------------------------------------------------------
 *
 * ic_proxy_iobuf.h
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#ifndef IC_PROXY_IOBUF_H
#define IC_PROXY_IOBUF_H


#include "postgres.h"


typedef struct ICProxyOBuf ICProxyOBuf;
typedef struct ICProxyIBuf ICProxyIBuf;


typedef void (* ic_proxy_iobuf_data_callback) (void *opaque,
											   const void *data,
											   uint16 size);


struct ICProxyIBuf
{
	char	   *buf;
	uint16		len;

	uint16		header_size;
	uint16		(* get_packet_size) (const void *data);
};

struct ICProxyOBuf
{
	char	   *buf;
	uint16		len;

	uint16		header_size;
	void		(* set_packet_size) (void *data, uint16 size);
};


extern void ic_proxy_obuf_init(ICProxyOBuf *obuf, uint16 header_size,
							   void (* set_packet_size) (void *data,
														 uint16 size));
extern void ic_proxy_obuf_init_b2c(ICProxyOBuf *obuf);
extern void ic_proxy_obuf_init_p2p(ICProxyOBuf *obuf);
extern void ic_proxy_obuf_uninit(ICProxyOBuf *obuf);
extern void *ic_proxy_obuf_ensure_buffer(ICProxyOBuf *obuf);
extern void ic_proxy_obuf_push(ICProxyOBuf *obuf,
							   const char *data, uint16 size,
							   ic_proxy_iobuf_data_callback callback,
							   void *opaque);


extern void ic_proxy_ibuf_init(ICProxyIBuf *ibuf, uint16 header_size,
							   uint16 (* get_packet_size) (const void *data));
extern void ic_proxy_ibuf_init_b2c(ICProxyIBuf *ibuf);
extern void ic_proxy_ibuf_init_p2p(ICProxyIBuf *ibuf);
extern void ic_proxy_ibuf_uninit(ICProxyIBuf *ibuf);
extern void ic_proxy_ibuf_clear(ICProxyIBuf *ibuf);
extern bool ic_proxy_ibuf_empty(const ICProxyIBuf *ibuf);
extern void ic_proxy_ibuf_push(ICProxyIBuf *ibuf,
							   const char *data, uint16 size,
							   ic_proxy_iobuf_data_callback callback,
							   void *opaque);

#endif   /* IC_PROXY_IOBUF_H */
