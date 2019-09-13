

remove_existing_symlink_to_installation() {
  local installation_link_name=$1
  rm -f $installation_link_name
}

#
# Helpers
#
create_symlink_to_installation() {
  local gpdb_installation_path=$1
  local link_name=$2

  ln -s $gpdb_installation_path $link_name
}

remove_gphdfs_permissions() {
	psql postgres -c "alter role $USER NOCREATEEXTTABLE(protocol='gphdfs',type='readable');"
	psql postgres -c "alter role $USER NOCREATEEXTTABLE(protocol='gphdfs',type='writable');"
}

create_demo_cluster() {
  local gpdb_source_path=$1;
  local installation_path=$2;
  local gpdb_version=$3;
  
  source "./configuration/$gpdb_version-env.sh" && \
    source "./$installation_path/greenplum_path.sh" && \
    make -C "$gpdb_source_path/gpAux/gpdemo" && \
    remove_gphdfs_permissions && \
    "./$installation_path/bin/gpstop" -a;
}

create_backup_of_data_dirs() {
  local source_directory=$1
  local backup_directory=$2

  rm -rf "$backup_directory"
  cp -r "$source_directory" "$backup_directory"
}

#
# Main body: takes two arguments
#
# - gpdb installation path: the path to an installed GPDB
# - gpdb source path: the path to a gpdb source tree containing gpdemo
#
init_cluster() {
  local gpdb_installation_path=$1
  local gpdb_source_path=$2
  local gpdb_version=$3

  local gpdb_installation_link_name="$gpdb_version"
  local data_directory="$gpdb_version-data"
  local backup_data_directory="$gpdb_version-data-copy"

  remove_existing_symlink_to_installation "$gpdb_installation_link_name"
  create_symlink_to_installation "$gpdb_installation_path" "$gpdb_installation_link_name"
  create_demo_cluster "$gpdb_source_path" "$gpdb_installation_link_name" "$gpdb_version"
  
  echo ""
  echo "Creating back up of data directory from $data_directory to $backup_data_directory."
  create_backup_of_data_dirs "$data_directory" "$backup_data_directory"
  echo "Done."
}