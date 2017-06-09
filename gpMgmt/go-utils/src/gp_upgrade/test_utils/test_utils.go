package test_utils

import (
	"fmt"
	"gp_upgrade/config"
	"io/ioutil"
	"os"
)

const (
	TempHomeDir = "/tmp/gp_upgrade_test_temp_home_dir"

	SAMPLE_JSON = `[{
    "address": "briarwood",
    "content": 2,
    "dbid": 7,
    "fsedbid": 7,
    "fsefsoid": "3052",
    "fselocation": "/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/dbfast_mirror3/demoDataDir2",
    "hostname": "briarwood",
    "mode": "s",
    "port": 25437,
    "preferred_role": "m",
    "replication_port": 25443,
    "role": "m",
    "san_mounts": null,
    "status": "u"
  }]`
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

func WriteSampleConfig() {
	err := os.MkdirAll(config.GetConfigDir(), 0700)
	Check("cannot create sample dir", err)
	err = ioutil.WriteFile(config.GetConfigFilePath(), []byte(SAMPLE_JSON), 0600)
	Check("cannot write sample config", err)
}
