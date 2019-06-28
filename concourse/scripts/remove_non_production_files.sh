#!/usr/bin/env bash

set -euxo pipefail

main() {
  local tmp_dir="$(mktemp -d)"
  local wd=$(pwd)

  tar zxf "${INPUT_TARBALL}" -C "${tmp_dir}"

  cd "${tmp_dir}"
  xargs rm -v < "${wd}/${NON_PRODUCTION_FILES}" || true
  if ! tar czf "${wd}/${OUTPUT_TARBALL}" ./* ; then
    echo "All files were removed so ${OUTPUT_TARBALL} cannot be created"
    exit 1
  fi
}

main "$@"
