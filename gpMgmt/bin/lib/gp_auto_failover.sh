#!/usr/bin/env bash

CONFIG_COORDINATOR_AUTO_FAILOVER() {
if [ x"" == x"$AUTOFAILOVER_CONFIG" ]; then
    return
fi

LOG_MSG "[INFO]:-Start to configure coordinator auto failover" 1
LOG_MSG "[INFO]:-Start to configure the monitor"

LOG_MSG "[INFO]: INTERACTIVE = $INTERACTIVE" 1
# get configuration from gp_segment_configuration
LOG_MSG "[INFO]:-MONITOR_HOST = '$MONITOR_HOST'" 1
LOG_MSG "[INFO]:-MONITOR_PORT = '$MONITOR_PORT'" 1
LOG_MSG "[INFO]:-MONITOR_DATADIR = '$MONITOR_DATADIR'" 1
if [ -z "$MONITOR_PORT" || -z "$MONITOR_DATADIR" ]; then
    LOG_MSG "[ERROR]: monitor port/datadir is empty" 1
    exit 1
fi
local opts=""
if [ x"" == x"$INTERACTIVE" ]; then
    opts="-a"
fi
gpautoctl -m config -M $MONITOR_HOST:$MONITOR_PORT -D $MONITOR_DATADIR -c :$MASTER_PORT $opts

}
