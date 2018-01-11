"""
Copyright (c) 2004-Present Pivotal Software, Inc.

This program and the accompanying materials are made available under
the terms of the under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""

import signal

class TimeoutException(Exception):
    """
    Exception that is thrown when a timeout occurs.
    """
    pass

class Timeout(object):
    """
    A class that implements timeout using ALARM signal.
    Wrap any python call using the with(Timeout(seconds))
    which will raise a TimoutException if the python call
    does not finish within 'seconds'
    """

    def __init__(self, sec, cmd=None):
        self.sec = sec
        self.cmd = cmd

    def __enter__(self):
        signal.signal(signal.SIGALRM, self.raise_timeout)
        signal.alarm(self.sec)

    def __exit__(self, *args):
        signal.alarm(0)    # disable alarm

    def raise_timeout(self, *args):
        if self.cmd:
            self.cmd.cancel()
        raise TimeoutException()

