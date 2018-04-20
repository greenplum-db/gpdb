package configutils

import (
	"path/filepath"
)

//"address": "briarwood",
//"content": 2,
//"dbid": 7,
//"datadir": "/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/dbfast_mirror3/demoDataDir2",
//"hostname": "briarwood",
//"mode": "s",
//"port": 25437,
//"preferred_role": "m",
//"role": "m",
//"status": "u"

type SegmentConfiguration []Segment

type Segment struct {
	Address       string `json:"address"`
	Content       int    `json:"content"`
	Datadir       string `json:"datadir"`
	Dbid          int    `json:"dbid"`
	Hostname      string `json:"hostname"`
	Mode          string `json:"mode"`
	Status        string `json:"status"`
	Port          int    `json:"port"`
	PreferredRole string `json:"preferred_role" db:"preferred_role"`
	Role          string `json:"role"`
}

func GetConfigFilePath(base string) string {
	return filepath.Join(base, "cluster_config.json")
}

func GetVersionFilePath(base string) string {
	return filepath.Join(base, "cluster_version.json")
}

func GetNewClusterConfigFilePath(base string) string {
	return filepath.Join(base, "new_cluster_config.json")
}
