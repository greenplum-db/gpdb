/*-------------------------------------------------------------------------
 *
 * ic_proxy_iobuf.c
 *
 *	  Interconnect Proxy I/O Buffer
 *
 * An ibuf detects the boundaries of input packets and generate a callback for
 * every complete packet.
 *
 * An obuf combines small output packets into large ones, which helps to reduce
 * the overhead of packet headers.
 *
 * Both i/o bufs are designed to support any kind of packets, as long as they
 * have a fixed-size packet header, and the maximum packet size does not exceed
 * IC_PROXY_MAX_PKT_SIZE.  Two built-in packet formats are provided:
 *
 * - b2c packet: the the ic-tcp packet, which contains a 4-byte header;
 * - p2p packet: the ic-proxy packet, which contains a 32-byte header;
 *
 * Other formats can be supported by providing custom methods.
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "cdb/ml_ipc.h"					/* for ic-tcp packet */

#include "ic_proxy.h"
#include "ic_proxy_iobuf.h"
#include "ic_proxy_packet.h"
#include "ic_proxy_pkt_cache.h"


/*
 * Initialize an ibuf.
 *
 * The packet format must has a fixed-size header, which is specified by
 * header_size.  Once a complete header is received the get_packet_size()
 * method is called to parse the packet size, including both the header and the
 * body.
 */
void
ic_proxy_ibuf_init(ICProxyIBuf *ibuf, uint16 header_size,
				   uint16 (* get_packet_size) (const void *data))
{
	ibuf->len = 0;
	ibuf->buf = NULL;

	ibuf->header_size = header_size;
	ibuf->get_packet_size = get_packet_size;
}

/*
 * Uninitialize an ibuf.
 */
void
ic_proxy_ibuf_uninit(ICProxyIBuf *ibuf)
{
	if (ibuf->buf)
	{
		ic_proxy_pkt_cache_free(ibuf->buf);
		ibuf->buf = NULL;
	}
}

/*
 * Clear the ibuf.
 *
 * Unconsumed bytes are dropped directly.
 */
void
ic_proxy_ibuf_clear(ICProxyIBuf *ibuf)
{
	if (ibuf->len > 0)
		elog(WARNING, "ic-proxy: dropped %d bytes", ibuf->len);

	ibuf->len = 0;
}

/*
 * Return true if the ibuf is empty.
 */
bool
ic_proxy_ibuf_empty(const ICProxyIBuf *ibuf)
{
	return ibuf->len == 0;
}

/*
 * Push data to the ibuf.
 *
 * The "data" and "size" are the pointer and the size of the data, they are
 * appended to the "ibuf".  If a complete packet is composed the "callback" is
 * called with the "opaque" as the callback data.
 *
 * If "size" is 0 then a force flush is triggered, the "callback" is called
 * with the incomplete packet.
 *
 * TODO: libuv supports sending an array of buffers, so it is technically
 * possible to prevent the memory copying in this function, we only need to let
 * the get_packet_size() method supports parsing the packet size from an array
 * of buffers.  However the callback function also needs to support
 * non-continues packet, an easy way is to only copying the header into
 * continues memory.
 */
void
ic_proxy_ibuf_push(ICProxyIBuf *ibuf,
				   const char *data, uint16 size,
				   ic_proxy_iobuf_data_callback callback,
				   void *opaque)
{
	uint16		packet_size;
	uint16		delta;

	if (unlikely(ibuf->buf == NULL))
		ibuf->buf = ic_proxy_pkt_cache_alloc(NULL);

	/* a force-flush */
	if (unlikely(size == 0))
	{
		/* TODO: do we need to flush if ibuf->len is 0? */
		callback(opaque, ibuf->buf, ibuf->len);
		ibuf->len = 0;
		return;
	}

	if (ibuf->len > 0)
	{
		if (ibuf->len < ibuf->header_size)
		{
			/* haven't got a complete header yet */

			delta = Min(ibuf->header_size - ibuf->len, size);

			memcpy(ibuf->buf + ibuf->len, data, delta);
			ibuf->len += delta;
			data += delta;
			size -= delta;

			if (ibuf->len < ibuf->header_size)
				/* still not having a complete header */
				return;
		}

		{
			/* have a complete header now */

			packet_size = ibuf->get_packet_size(ibuf->buf);
			if (packet_size > ibuf->len)
			{
				delta = Min(packet_size - ibuf->len, size);
				memcpy(ibuf->buf + ibuf->len, data, delta);
				ibuf->len += delta;
				data += delta;
				size -= delta;
			}
			if (ibuf->len < packet_size)
				/* still not having a complete packet */
				return;
		}

		{
			/* have a complete pkt now */

			callback(opaque, ibuf->buf, packet_size);
			ibuf->len = 0;
		}
	}

	while (size >= ibuf->header_size)
	{
		packet_size = ibuf->get_packet_size(data);

		if (packet_size > 0 && packet_size <= size)
		{
			/* got a complete pkt */
			callback(opaque, data, packet_size);

			data += packet_size;
			size -= packet_size;
		}
		else
			/* got a incomplete pkt */
			break;
	}

	if (size > 0)
	{
		/* got a incomplete pkt */
		memcpy(ibuf->buf, data, size);
		ibuf->len = size;
	}
}

/*
 * Get the packet size of a b2c one.
 *
 * The b2c packet only contains a 4-byte packet length in host byte-order.
 */
static uint16
ic_proxy_ibuf_get_packet_size_b2c(const void *data)
{
	return *(const uint32 *) data;
}

/*
 * Initialize an ibuf for b2c packet.
 */
void
ic_proxy_ibuf_init_b2c(ICProxyIBuf *ibuf)
{
	ic_proxy_ibuf_init(ibuf, PACKET_HEADER_SIZE,
					   ic_proxy_ibuf_get_packet_size_b2c);
}

/*
 * Get the packet size of a p2p one.
 *
 * The p2p packet contains a 32-byte ICProxyPkt header, all the fields are in
 * host byte-order.
 */
static uint16
ic_proxy_ibuf_get_packet_size_p2p(const void *data)
{
	const ICProxyPkt *pkt = data;

	return pkt->len;
}

/*
 * Initialize an ibuf for p2p packet.
 */
void
ic_proxy_ibuf_init_p2p(ICProxyIBuf *ibuf)
{
	ic_proxy_ibuf_init(ibuf, sizeof(ICProxyPkt),
					   ic_proxy_ibuf_get_packet_size_p2p);
}


/*
 * Initialize an obuf.
 *
 * The packet format must has a fixed-size header, which is specified by
 * header_size.  Once a packet is to be sent, the set_packet_size() method is
 * called to set the packet size in the header.
 *
 * The ic_proxy_obuf_ensure_buffer() must be called after init and before any
 * data is pushed.
 */
void
ic_proxy_obuf_init(ICProxyOBuf *obuf, uint16 header_size,
				   void (* set_packet_size) (void *data, uint16 size))
{
	obuf->len = 0;
	obuf->buf = NULL;

	obuf->header_size = header_size;
	obuf->set_packet_size = set_packet_size;
}

/*
 * Uninitialize an obuf.
 */
void
ic_proxy_obuf_uninit(ICProxyOBuf *obuf)
{
	if (obuf->buf)
	{
		ic_proxy_pkt_cache_free(obuf->buf);
		obuf->buf = NULL;
	}
}

/*
 * Ensure that the obuf has allocated its buffer.
 *
 * The ibuf does not allocate buffer until this function is called, this helps
 * to prevent some unnecessary memory allocation.
 *
 * The buffer pointer is returned, the caller is responsible to initialize the
 * header.  The packet size, however, should be set in the set_packet_size()
 * method.
 *
 * TODO: This used to be useful to improve the performance slightly, however as
 * we have the packet cache already, we do not need this lazy allocation
 * anymore, we could retire it.
 */
void *
ic_proxy_obuf_ensure_buffer(ICProxyOBuf *obuf)
{
	if (unlikely(obuf->buf == NULL))
	{
		obuf->buf = ic_proxy_pkt_cache_alloc(NULL);
		obuf->len = obuf->header_size;
	}

	return obuf->buf;
}

/*
 * Push data to the obuf.
 *
 * The "data" and "size" are the pointer and the size of the data, it is
 * promised that the data is always fed to the "callback" as an entity.  If
 * there is no enough room for the "data", the bytes in the buffer will first
 * be fed to the "callback" to clear the room.
 *
 * If "size" is 0 then a force flush is triggered.
 *
 * The "callback" will be called with one or more complete data.  The output
 * packet header is always set before feeding to the "callback".
 */
void
ic_proxy_obuf_push(ICProxyOBuf *obuf,
				   const char *data, uint16 size,
				   ic_proxy_iobuf_data_callback callback,
				   void *opaque)
{
	if (unlikely(obuf->buf == NULL))
		elog(ERROR,
					 "ic-proxy: the caller must init the header before pushing data");

	/*
	 * Need a flush when:
	 * - size == 0 means a force flush;
	 * - or no enough space for the new data;
	 */
	if (unlikely(size == 0 || size + obuf->len > IC_PROXY_MAX_PKT_SIZE))
	{
		if (obuf->header_size + size > IC_PROXY_MAX_PKT_SIZE)
			elog(ERROR,
						 "ic-proxy: not enough buffer to store the data:"
						 " the data size is %d bytes,"
						 " but the buffer size is only %zd bytes,"
						 " including a %d bytes header",
						 size, IC_PROXY_MAX_PKT_SIZE, obuf->header_size);

		/* TODO: should we flush if no data in the packet? */
		if (obuf->len == obuf->header_size)
		{
			elogif(gp_log_interconnect >= GPVARS_VERBOSITY_DEBUG, DEBUG1,
				   "ic-proxy: no data to flush");
		}
		else
		{
			obuf->set_packet_size(obuf->buf, obuf->len);
			callback(opaque, obuf->buf, obuf->len);

			/* we will reuse the header */
			obuf->len = obuf->header_size;
		}
	}

	/* the trailing data will be sent later */
	if (size > 0)
	{
		memcpy(obuf->buf + obuf->len, data, size);
		obuf->len += size;
	}
}

/*
 * Set the packet size of a b2c one.
 *
 * The b2c packet only contains a 4-byte packet length in host byte-order.
 */
static void
ic_proxy_obuf_set_packet_size_b2c(void *data, uint16 size)
{
	*(uint32 *) data = size;
}

/*
 * Initialize an obuf for b2c packet.
 */
void
ic_proxy_obuf_init_b2c(ICProxyOBuf *obuf)
{
	ic_proxy_obuf_init(obuf, PACKET_HEADER_SIZE,
					   ic_proxy_obuf_set_packet_size_b2c);
}

/*
 * Set the packet size of a p2p one.
 *
 * The p2p packet contains a 32-byte ICProxyPkt header, all the fields are in
 * host byte-order.
 */
static void
ic_proxy_obuf_set_packet_size_p2p(void *data, uint16 size)
{
	ICProxyPkt *pkt = data;

	pkt->len = size;
}

/*
 * Initialize an obuf for p2p packet.
 */
void
ic_proxy_obuf_init_p2p(ICProxyOBuf *obuf)
{
	ic_proxy_obuf_init(obuf, sizeof(ICProxyPkt),
					   ic_proxy_obuf_set_packet_size_p2p);
}
