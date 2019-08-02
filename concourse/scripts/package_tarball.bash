#!/bin/bash

set -eux

main() {
  tar -czf "$dst_tarball" -C "$src_root" .
}

main "$@"
