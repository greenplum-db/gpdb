#!/bin/bash

set -u -e -x

main() {
  tar czf "$dst_tarball" -C "$src_root" .
}

main "$@"
