#!/usr/bin/env python

import os
import cPickle
import time
import sys

#communicate with another process through named pipe

#one for receive command, the other for send command

data_file = sys.argv[1]
tmp_fname = "/tmp/%s" % os.path.split(data_file)[-1]

with open(tmp_fname, 'r') as rp:
    response = rp.read()

with open(data_file, 'w') as fp:
    fp.write(response)
