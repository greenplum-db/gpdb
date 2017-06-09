package test_utils

import (
	"fmt"
	"os"
)

const (
	TempHomeDir = "/tmp/gp_upgrade_test_temp_home_dir"
)

func Check(msg string, e error) {
	if e != nil {
		panic(fmt.Sprintf("%s: %s\n", msg, e.Error()))
	}
}

func SetHomeDir(temp_home_dir string) string {
	save := os.Getenv("HOME")
	err := os.MkdirAll(temp_home_dir, 0700)
	Check("cannot create home temp dir", err)
	err = os.Setenv("HOME", temp_home_dir)
	Check("cannot set home dir", err)
	return save
}

func ResetTempHomeDir() string {
	err := os.RemoveAll(TempHomeDir)
	Check("cannot remove temp home", err)
	return SetHomeDir(TempHomeDir)
}
