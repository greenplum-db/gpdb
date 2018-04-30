package cluster

import (
	"fmt"

	"gp_upgrade/helpers"
	"gp_upgrade/hub/configutils"
	"gp_upgrade/hub/upgradestatus"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
)

type Pair struct {
	upgradeConfig          configutils.UpgradeConfig
	oldMasterPort          int
	newMasterPort          int
	oldMasterDataDirectory string
	newMasterDataDirectory string
	oldBinDir              string
	newBinDir              string
	commandExecer          helpers.CommandExecer
}

func (cp *Pair) Init(baseDir, oldBinDir, newBinDir string, execer helpers.CommandExecer) error {
	var err error
	cp.oldBinDir = oldBinDir
	cp.newBinDir = newBinDir
	cp.commandExecer = execer

	cp.upgradeConfig, err = configutils.GetUpgradeConfig(baseDir)
	if err != nil {
		return fmt.Errorf("couldn't read config files: %+v", err)
	}

	cp.oldMasterPort, cp.newMasterPort, err = cp.upgradeConfig.GetMasterPorts()
	if err != nil {
		return err
	}

	cp.oldMasterDataDirectory, cp.newMasterDataDirectory, err = cp.upgradeConfig.GetMasterDataDirs()
	if err != nil {
		return err
	}

	return nil
}

func (cp *Pair) StopEverything(pathToGpstopStateDir string) {
	checklistManager := upgradestatus.NewChecklistManager(pathToGpstopStateDir)
	checklistManager.ResetStateDir("gpstop.old")
	checklistManager.ResetStateDir("gpstop.new")

	oldGpstopShellArgs := fmt.Sprintf("source %s/../greenplum_path.sh; %s/gpstop -a -d %s",
		cp.oldBinDir, cp.oldBinDir, cp.oldMasterDataDirectory)
	runOldStopCmd := cp.commandExecer("bash", "-c", oldGpstopShellArgs)
	gplog.Info("old gpstop command: %+v", runOldStopCmd)

	stopCluster(runOldStopCmd, "gpstop.old", checklistManager)

	newGpstopShellArgs := fmt.Sprintf("source %s/../greenplum_path.sh; %s/gpstop -a -d %s",
		cp.newBinDir, cp.newBinDir, cp.newMasterDataDirectory)
	runNewStopCmd := cp.commandExecer("bash", "-c", newGpstopShellArgs)
	gplog.Info("new gpstop command: %+v", runNewStopCmd)

	stopCluster(runNewStopCmd, "gpstop.new", checklistManager)
}

func stopCluster(stopCmd helpers.Command, step string, stateManager *upgradestatus.ChecklistManager) {
	err := stateManager.MarkInProgress(step)
	if err != nil {
		gplog.Error(err.Error())
		return
	}

	err = stopCmd.Run()

	gplog.Info("finished stopping %s", step)
	if err != nil {
		gplog.Error(err.Error())
		stateManager.MarkFailed(step)
		return
	}

	stateManager.MarkComplete(step)
}

func (cp *Pair) GetPortsAndDataDirForReconfiguration() (int, int, string) {
	return cp.oldMasterPort, cp.newMasterPort, cp.newMasterDataDirectory
}
