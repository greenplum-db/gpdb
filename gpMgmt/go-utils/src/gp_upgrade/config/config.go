package config

import "os"

//"address": "briarwood",
//"content": 2,
//"dbid": 7,
//"fsedbid": 7,
//"fsefsoid": "3052",
//"fselocation": "/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/dbfast_mirror3/demoDataDir2",
//"hostname": "briarwood",
//"mode": "s",
//"port": 25437,
//"preferred_role": "m",
//"replication_port": 25443,
//"role": "m",
//"san_mounts": null,
//"status": "u"

type Configuration []ConfigRow

type ConfigRow struct {
	Address  string `json:"address"`
	Content  int    `json:"content"`
	DBID     int    `json:"dbid"`
	Hostname string `json:"hostname"`
	Port     int    `json:"port"`
}

func GetConfigDir() string {
	return os.Getenv("HOME") + "/.gp_upgrade"
}

func GetConfigFilePath() string {
	return GetConfigDir() + "/cluster_config.json"
}
