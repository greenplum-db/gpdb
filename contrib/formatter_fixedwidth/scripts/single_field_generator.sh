#!/bin/bash
#
# Portions Copyright (c) 2022 VMware, Inc. or its affiliates.
#
# Generate a single field large file test dataset.
# It's used by readable_query32 test case and could avoid uploading a large file.

for x in {40001..50000};do
    printf "%010d\n" $x;
done
