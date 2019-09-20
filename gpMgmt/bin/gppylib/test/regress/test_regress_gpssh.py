#!/usr/bin/env python

import os, signal, time, re
import unittest
import psutil
try:
    from subprocess32 import PIPE
except:
    from subprocess import PIPE

class GpsshTestCase(unittest.TestCase):

    # return count of stranded ssh processes 
    def searchForProcessOrChildren(self):

        euid = os.getuid()
        count = 0

        for p in psutil.process_iter():
            if p.uids().effective != euid:
                continue
            if not re.search('ssh', ' '.join(p.cmdline())):
                continue
            if p.ppid() != 1:
                continue

            count += 1

        return count


    def test00_gpssh_sighup(self):
        """Verify that gppsh handles sighup 
           and terminates cleanly.
        """

        before_count = self.searchForProcessOrChildren()

        p = psutil.Popen("gpssh -h localhost", shell=True, stdin=PIPE, stdout=PIPE, stderr=PIPE)
        pid = p.pid

        time.sleep(3)

        try:
            os.kill(int(pid), signal.SIGHUP)
        except Exception:
            pass

        max_attempts = 6
        for i in range(max_attempts):
            after_count = self.searchForProcessOrChildren()
            error_count = after_count - before_count
            if error_count:
                if (i + 1) == max_attempts:
                    self.fail("Found %d new stranded gpssh processes after issuing sig HUP" % error_count)
                time.sleep(.5)
 
if __name__ == "__main__":
    unittest.main()
