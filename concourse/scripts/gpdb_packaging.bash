#!/bin/bash -l

set -euxo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

substitute_GP_VERSION() {
  GP_VERSION=$("$DIR/../../getversion" --short)
  INSTALLER_ZIP=${INSTALLER_ZIP//@GP_VERSION@/${GP_VERSION}}
}

function echo_expected_env_variables() {
  echo "$INSTALL_SCRIPT_SRC"
  echo "$GPDB_TARGZ"
  echo "$INSTALLER_ZIP"
}

function _main() {
  substitute_GP_VERSION
  echo_expected_env_variables

  # Copy gpaddon into addon to ensure the availability of all the installer scripts
  cp -R gpaddon_src gpdb_src/gpAux/addon

  local installer_bin
  installer_bin=$( echo "$INSTALLER_ZIP" | sed "s/.zip/.bin/" | xargs basename)

  GP_VERSION=$("${DIR}/../../getversion" --short)
  sed -i \
      -e "s:\(installPath=/usr/local/GP-\).*:\1$GP_VERSION:" \
      -e "s:\(installPath=/usr/local/greenplum-db-\).*:\1$GP_VERSION:" \
      "$INSTALL_SCRIPT_SRC"

  cat "$INSTALL_SCRIPT_SRC" "$GPDB_TARGZ" > "$installer_bin"
  chmod a+x "$installer_bin"
  zip "$INSTALLER_ZIP" "$installer_bin"
  openssl dgst -sha256 "$INSTALLER_ZIP" > "$INSTALLER_ZIP".sha256
}

_main "$@"
