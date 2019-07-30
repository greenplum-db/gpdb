#!/usr/bin/env bats

TEST_DIR="test_dir"

setup() {
  mkdir "${TEST_DIR}"
  cd "${TEST_DIR}"

  export dst_tarball="dst.tar.gz"
  export src_root="src_root"

  mkdir "${src_root}"
  touch "${src_root}/"{a,b,c}
}

teardown() {
  cd ..
  rm -rf "${TEST_DIR}"
}

main() {
  ../scripts/package_tarball.bash
}

@test "it fails if any variables are unbound" {
  unset src_root dst_tarball

  run main
  [ "${status}" -ne 0 ]
}

@test "it fails when src_root is a non-existent directory" {
  export src_root="not-a-file"

  run main
  [ "${status}" -ne 0 ]
}

@test "it works when src_root is empty" {
  rm -rf "${src_root}/*"

  run main
  [ "${status}" -eq 0 ]
}

@test "it properly tars file in src_root/" {
  run main
  [ "${status}" -eq 0 ]

  run tar tf "$dst_tarball"
  [ "${status}" -eq 0 ]
  [[ "${output}" =~ a ]]
  [[ "${output}" =~ b ]]
  [[ "${output}" =~ c ]]
}
