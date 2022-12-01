#!/bin/bash -l

source /usr/local/greenplum-db-devel/greenplum_path.sh

delta=-3000

psql -tqA -d postgres -P pager=off -F: -R, \
    -c "select dbid, content, address, port$delta as port
          from gp_segment_configuration
         order by 1" \
| xargs -rI'{}' \
  gpconfig --skipvalidation -c gp_interconnect_proxy_addresses -v "'{}'"

# also have to enlarge gp_interconnect_tcp_listener_backlog
gpconfig -c gp_interconnect_tcp_listener_backlog -v 1024

gpstop -u