package utils

import (
	"fmt"
	"os"
)

func Check(msg string, e error) {
	if e != nil {
		panic(fmt.Sprintf("%s: %s\n", msg, e.Error()))
	}
}

func SetHomeDir(temp_home_dir string) string {
	save := os.Getenv("HOME")
	err := os.RemoveAll(temp_home_dir)
	Check("cannot remove home temp dir", err)
	err = os.MkdirAll(temp_home_dir, 0700)
	Check("cannot create home temp dir", err)
	err = os.Setenv("HOME", temp_home_dir)
	Check("cannot set home dir", err)
	return save
}
