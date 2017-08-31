#! /usr/bin/env python

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

import sys
import os
import threading
from tinctest.lib import local_path

timeout_count = 100
interval_seconds = 1.0
count = 0

def is_timeout():
    global count, timeout_count
    if count >= timeout_count:
        return True
    return False

def is_finish():
    if os.path.exists('/tmp/gdb_sync'):
        return True
    return False

def check_sync():
    global count, interval_seconds
    count = count + 1
    if is_finish() or is_timeout():
        return
    threading.Timer(interval_seconds, check_sync).start()

check_sync()
