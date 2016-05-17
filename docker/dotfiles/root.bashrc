#!/bin/bash

if [ ! -e /tmp/ssh_sem ]; then
  touch /tmp/ssh_sem
  /workspace/gpdb/docker/start_ssh.bash
fi
