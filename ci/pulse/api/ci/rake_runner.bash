#!/bin/bash

set -euxo pipefail

main() {
  local workdir=$1

  pushd "$workdir"
    bundle install
    bundle exec rake
  popd
}

main "$@"
