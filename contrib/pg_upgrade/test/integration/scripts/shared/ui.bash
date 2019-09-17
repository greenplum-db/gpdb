validate_gpdb_installation_path() {
	if [ ! -d "$1" ]; then
		echo "missing argument: installation path"
		usage "$2"
		exit 1
	fi
}

validate_gpdb_source_path() {
	if [ ! -d "$1" ]; then
		echo "missing argument: source path"
		usage "$2"
		exit 1
	fi
}

usage() {
	local gpdb_version=$1

	cat <<USAGE

usage:

./scripts/init-$gpdb_version-cluster.bash [INSTALLATION_PATH] [SOURCE_PATH]

example:

./scripts/init-$gpdb_version-cluster.bash \
  $HOME/workspace/$gpdb_version/gpAux/greenplum-db-installation \
  $HOME/workspace/$gpdb_version;
USAGE
}
