#!/usr/bin/env python3

import time
import signal
import os
import sys

signal.signal(signal.SIGTERM, signal.SIG_IGN)
signal.signal(signal.SIGHUP, signal.SIG_IGN)
signal.signal(signal.SIGINT, signal.SIG_IGN)
signal.signal(signal.SIGPIPE, signal.SIG_IGN)

with open(sys.argv[1], "w") as f:
    f.write(str(os.getpid()) + "\n")
    f.flush()
    os.fsync(f.fileno())

while True:
   time.sleep(1)
