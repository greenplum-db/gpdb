#!/usr/bin/env bash

set -e

./scripts/build-image.sh
./scripts/push.sh
git push
