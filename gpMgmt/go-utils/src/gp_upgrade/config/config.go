package config

import "os"

func GetConfigDir() string {
	return os.Getenv("HOME") + "/.gp_upgrade"
}

func GetConfigFilePath() string {
	return GetConfigDir() + "/cluster_config.json"
}
