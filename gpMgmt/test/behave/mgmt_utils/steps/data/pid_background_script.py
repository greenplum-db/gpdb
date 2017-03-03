 #!/usr/bin/env python

import time
import signal

signal.signal(signal.SIGTERM, signal.SIG_IGN)
signal.signal(signal.SIGHUP, signal.SIG_IGN)
signal.signal(signal.SIGINT, signal.SIG_IGN)
signal.signal(signal.SIGPIPE, signal.SIG_IGN)

while True:
   time.sleep(1)
