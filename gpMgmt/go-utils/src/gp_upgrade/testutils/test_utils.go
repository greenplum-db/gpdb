package testutils

import (
	"fmt"
	"io/ioutil"
	"net"
	"os"

	"gp_upgrade/hub/configutils"
)

const (
	SAMPLE_JSON = `[{
    "address": "briarwood",
    "content": 2,
    "datadir": "/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/dbfast_mirror3/demoDataDir2",
    "dbid": 7,
    "hostname": "briarwood",
    "mode": "s",
    "port": 25437,
    "preferred_role": "m",
    "role": "m",
    "status": "u"
  },
  {
    "address": "aspen",
    "content": 1,
    "datadir": "/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/dbfast_mirror2/demoDataDir1",
    "dbid": 6,
    "hostname": "aspen.pivotal",
    "mode": "s",
    "port": 25436,
    "preferred_role": "m",
    "role": "m",
    "status": "u"
  }]`

	MASTER_ONLY_JSON = `[{
		"address": "briarwood",
		"content": -1,
		"datadir": "/old/datadir",
		"dbid": 1,
		"hostname": "briarwood",
		"mode": "s",
		"port": 25437,
		"preferred_role": "m",
		"role": "m",
		"san_mounts": null,
		"status": "u"
	}]
`

	NEW_MASTER_JSON = `[{
		"address": "aspen",
		"content": -1,
		"datadir": "/new/datadir",
		"dbid": 1,
		"hostname": "briarwood",
		"mode": "s",
		"port": 35437,
		"preferred_role": "m",
		"role": "m",
		"san_mounts": null,
		"status": "u"
	}]
`
)

func Check(msg string, e error) {
	if e != nil {
		panic(fmt.Sprintf("%s: %s\n", msg, e.Error()))
	}
}

func WriteSampleConfig(base string) {
	WriteOldConfig(base, SAMPLE_JSON)
}

func WriteOldConfig(base, jsonConfig string) {
	err := os.MkdirAll(base, 0700)
	Check("cannot create old sample dir", err)
	err = ioutil.WriteFile(configutils.GetConfigFilePath(base), []byte(jsonConfig), 0600)
	Check("cannot write old sample configutils", err)
}

func WriteNewConfig(base, jsonConfig string) {
	err := os.MkdirAll(base, 0700)
	Check("cannot create new sample dir", err)
	err = ioutil.WriteFile(configutils.GetNewClusterConfigFilePath(base), []byte(jsonConfig), 0600)
	Check("cannot write new sample configutils", err)
}

func GetOpenPort() (int, error) {
	addr, err := net.ResolveTCPAddr("tcp", "localhost:0")
	if err != nil {
		return 0, err
	}

	l, err := net.ListenTCP("tcp", addr)
	if err != nil {
		return 0, err
	}
	defer l.Close()

	return l.Addr().(*net.TCPAddr).Port, nil
}
