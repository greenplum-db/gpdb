#!/usr/bin/env bats

setup() {
  mkdir test_dir
  cd test_dir

  export OUTPUT_TARBALL="out.tar.gz"

  touch a b c

  tar --create --gzip --file a.tar.gz ./a
  tar czf ab.tar.gz ./{a,b}
  tar czf abc.tar.gz ./{a,b,c}

  echo -e ""        > empty-NON_PRODUCTION_FILES.txt
  echo -e "a"       > a-NON_PRODUCTION_FILES.txt
  echo -e "a\nb"    > ab-NON_PRODUCTION_FILES.txt
  echo -e "a\nb\nc" > abc-NON_PRODUCTION_FILES.txt
}

teardown() {
  cd ..
  rm -rf test_dir
}

comm_tarballs() {
  comm -3 <(tar -tf "$1") <(tar -tf "$2") | tr -d '[:space:]'
}

@test "test comm_tarballs with same file" {
  run comm_tarballs a.tar.gz a.tar.gz
  [ "$output" = "" ]
}

@test "test comm_tarballs with different file" {
  run comm_tarballs a.tar.gz ab.tar.gz
  [ "$output" = "./b" ]
}

main() {
  ../scripts/remove_non_production_files.sh
}

@test "it fails if any variables are unbound" {
  run main
  [ "$status" -ne 0 ]
}

@test "it prints out the removed files" {
  export INPUT_TARBALL="abc.tar.gz"
  export NON_PRODUCTION_FILES="ab-NON_PRODUCTION_FILES.txt"

  run main
  [ "$status" -eq 0 ]
  [[ "$output" =~ "rm".*$'\n'.*"a".*$'\n'.*b ]]
}

@test "it does nothing when there are no files to remove" {
  export INPUT_TARBALL="a.tar.gz"
  export NON_PRODUCTION_FILES="empty-NON_PRODUCTION_FILES.txt"

  run main
  [ "$status" -eq 0 ]

  run comm_tarballs "${INPUT_TARBALL}" "${OUTPUT_TARBALL}"
  [ -z "$output" ]
}

@test "it works when we remove a from ab" {
  export INPUT_TARBALL="ab.tar.gz"
  export NON_PRODUCTION_FILES="a-NON_PRODUCTION_FILES.txt"

  run main
  [ "$status" -eq 0 ]

  run comm_tarballs "${INPUT_TARBALL}" "${OUTPUT_TARBALL}"
  [ "$output" = "./a" ]
}

@test "it works when we remove ab from abc" {
  export INPUT_TARBALL="abc.tar.gz"
  export NON_PRODUCTION_FILES="ab-NON_PRODUCTION_FILES.txt"

  run main
  [ "$status" -eq 0 ]

  run comm_tarballs "${INPUT_TARBALL}" "${OUTPUT_TARBALL}"
  [ "$output" = "./a./b" ]
}

@test "it works when we remove abc from abc" {
  export INPUT_TARBALL="abc.tar.gz"
  export NON_PRODUCTION_FILES="abc-NON_PRODUCTION_FILES.txt"

  run main
  [ "$status" -ne 0 ]

  grep "All files were removed so ${OUTPUT_TARBALL} cannot be created" <(echo "$output")
  [ "$?" -eq 0 ]
}

@test "it fails when we remove ab from a" {
  export INPUT_TARBALL="a.tar.gz"
  export NON_PRODUCTION_FILES="ab-NON_PRODUCTION_FILES.txt"

  run main
  [ "$status" -ne 0 ]
  # rm: cannot remove `b': No such file or directory
  [[ "$output" =~ "rm".*"b".*"No such file or directory" ]]
}
