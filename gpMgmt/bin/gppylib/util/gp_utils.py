#!/usr/bin/env python3
#
# Copyright (c) Greenplum Inc 2008. All Rights Reserved.
#
# Greenplum DB related utility functions

import os

def get_gp_prefix(coordinatorDatadir):
    base = os.path.basename(coordinatorDatadir)
    idx = base.rfind('-1')
    if idx == -1:
        return None
    return base[0:idx]

