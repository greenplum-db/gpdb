#!/bin/bash

gpload -f test/behave/mgmt_utils/steps/data/gpload_merge_1.yml &
gpload -f test/behave/mgmt_utils/steps/data/gpload_merge_2.yml &
wait
