#!/bin/bash -l

set -euxo pipefail

DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

function echo_expected_env_variables() {
  echo "$INSTALL_SCRIPT_SRC"
  echo "$GPDB_TARGZ"
  echo "$INSTALLER_ZIP"
}

function _main() {
  echo_expected_env_variables
  local installer_bin
  installer_bin=$( echo "$INSTALLER_ZIP" | sed "s/.zip/.bin/" | xargs basename)

  source "${DIR}/../../../getversion"
  sed -i \
      -e "s:\(installPath=/usr/local/GP-\).*:\1$GP_VERSION:" \
      -e "s:\(installPath=/usr/local/greenplum-db-\).*:\1$GP_VERSION:" \
      "$INSTALL_SCRIPT_SRC"

  cat "$INSTALL_SCRIPT_SRC" "$GPDB_TARGZ" > "$installer_bin"
  chmod a+x "$installer_bin"
  zip "$INSTALLER_ZIP" "$installer_bin"
  openssl dgst -md5 "$INSTALLER_ZIP" > "$INSTALLER_ZIP".md5
}

_main "$@"
