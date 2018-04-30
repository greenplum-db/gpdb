package main

import (
	"fmt"
	"log"
	"os"
	"runtime/debug"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
	_ "github.com/lib/pq"
)

var (
	hubPort = "7527"
)

func main() {
	upgradePort := os.Getenv("GP_UPGRADE_HUB_PORT")
	if upgradePort != "" {
		hubPort = upgradePort
	}

	setUpLogging()

	addFlagOptions()

	confirmValidCommand()

	root.AddCommand(prepare, status, check, version, upgrade)

	prepare.AddCommand(subStartHub, subInitCluster, subShutdownClusters, subStartAgents)
	status.AddCommand(subUpgrade, subConversion)
	check.AddCommand(subVersion, subObjectCount, subDiskSpace, subConfig, subSeginstall)
	upgrade.AddCommand(subConvertMaster, subConvertPrimaries, subShareOids, subValidateStartCluster, subReconfigurePorts)

	err := root.Execute()
	if err != nil {
		// Use v to print the stack trace of an object errors.
		fmt.Printf("%+v\n", err)
		os.Exit(1)
	}
}

func confirmValidCommand() {
	if len(os.Args[1:]) < 1 {
		log.Fatal("Please specify one command of: check, prepare, status, upgrade, or version")
	}
}

func setUpLogging() {
	debug.SetTraceback("all")
	//empty logdir defaults to ~/gpAdminLogs
	gplog.InitializeLogging("gp_upgrade_cli", "")
}

func addFlagOptions() {
	addFlagOptionsToShutdownClusters()
	addFlagOptionsToInitCluster()
	addFlagOptionsToCheck()
	addFlagOptionsToConvertMaster()
	addFlagOptionsToValidateStartCluster()
	addFlagOptionsToConvertPrimaries()
}

func addFlagOptionsToConvertMaster() {
	subConvertMaster.Flags().StringVar(&oldDataDir, "old-datadir", "", "data directory for old gpdb version")
	subConvertMaster.MarkFlagRequired("old-datadir")
	subConvertMaster.Flags().StringVar(&oldBinDir, "old-bindir", "", "install directory for old gpdb version")
	subConvertMaster.MarkFlagRequired("old-bindir")
	subConvertMaster.Flags().StringVar(&newDataDir, "new-datadir", "", "data directory for new gpdb version")
	subConvertMaster.MarkFlagRequired("new-datadir")
	subConvertMaster.Flags().StringVar(&newBinDir, "new-bindir", "", "install directory for new gpdb version")
	subConvertMaster.MarkFlagRequired("new-bindir")
}

func addFlagOptionsToConvertPrimaries() {
	subConvertPrimaries.Flags().StringVar(&oldBinDir, "old-bindir", "", "install directory for old gpdb version")
	subConvertPrimaries.MarkFlagRequired("old-bindir")
	subConvertPrimaries.Flags().StringVar(&newBinDir, "new-bindir", "", "install directory for new gpdb version")
	subConvertPrimaries.MarkFlagRequired("new-bindir")
}

func addFlagOptionsToCheck() {
	check.PersistentFlags().StringVar(&masterHost, "master-host", "", "host IP for master")
	check.PersistentFlags().IntVar(&dbPort, "port", 15432, "port for Greenplum on master")
	check.MarkPersistentFlagRequired("master-host")
}

func addFlagOptionsToInitCluster() {
	subInitCluster.Flags().IntVar(&newClusterDbPort, "port", -1, "port for Greenplum on new master")
	subInitCluster.MarkFlagRequired("port")
}

func addFlagOptionsToShutdownClusters() {
	subShutdownClusters.Flags().StringVar(&oldBinDir, "old-bindir", "", "install directory for old gpdb version")
	subShutdownClusters.MarkFlagRequired("old-bindir")
	subShutdownClusters.Flags().StringVar(&newBinDir, "new-bindir", "", "install directory for new gpdb version")
	subShutdownClusters.MarkFlagRequired("new-bindir")
}

func addFlagOptionsToValidateStartCluster() {
	subValidateStartCluster.Flags().StringVar(&newDataDir, "new-datadir", "", "data directory for new gpdb version")
	subValidateStartCluster.MarkFlagRequired("new-datadir")
	subValidateStartCluster.Flags().StringVar(&newBinDir, "new-bindir", "", "install directory for new gpdb version")
	subValidateStartCluster.MarkFlagRequired("new-bindir")
}
