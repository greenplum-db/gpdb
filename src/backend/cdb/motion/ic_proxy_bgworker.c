/*-------------------------------------------------------------------------
 *
 * ic_proxy_bgworker.c
 *
 *    Interconnect Proxy Background Worker
 *
 * This is only a wrapper, the actual main loop is in ic_proxy_main.c .
 *
 *
 * Copyright (c) 2020-Present VMware, Inc. or its affiliates.
 *
 *
 *-------------------------------------------------------------------------
 */

#include "postgres.h"

#include "storage/ipc.h"

#include "cdb/ic_proxy_bgworker.h"
#include "ic_proxy_server.h"

bool
ICProxyStartRule(Datum main_arg)
{
	return true;
}

/*
 * ICProxyMain
 */
void
ICProxyMain(Datum main_arg)
{
	/* main loop */
	proc_exit(ic_proxy_server_main());
}
