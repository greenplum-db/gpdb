#! /usr/bin/env python
import psutil
import os
import resource

PERCENTAGE_OF_AVAIL_MEM_USED_FOR_THREADS = 0.8  # allow 20% of memory to remain for parent process

MB = 1024 * 1024

class SystemInfo():
    def __init__(self, logger=None):
        self.logger = logger
        self.pid = os.getpid()
        self.process = psutil.Process(self.pid)

    def print_mem_usage(self):
        mem = psutil.virtual_memory()
        print "available: %sMB percent: %s" % (mem.available >> 20, mem.percent)
        print "process memory usage %s" % (self.process.memory_info().vms >> 20)

    def debug_log_mem_usage(self):
        mem = psutil.virtual_memory()
        if self.logger:
            self.logger.debug('available: %sMB percent: %s' % (mem.available >> 20, mem.percent))
            self.logger.debug('process memory usage: %sMB' % (self.process.memory_info().vms >> 20))

def get_max_available_thread_count():
    stack_size, _ = resource.getrlimit(resource.RLIMIT_STACK)
    # assuming a generous 10K bytes per line of error output,
    # 20 MB allows 2000 errors in a single run; if user has more,
    # we will explain in the manual
    # the the user can always set batch (number of threads) manually
    thread_size = 20 * MB + stack_size

    mem = psutil.virtual_memory()
    available_mem = mem.available
    num_threads = int(available_mem / thread_size * PERCENTAGE_OF_AVAIL_MEM_USED_FOR_THREADS)
    return max(1, num_threads)
