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
	oldPostmasterRunning   bool
	newPostmasterRunning   bool
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

func convert(b bool) string {
	if b {
		return "is"
	}
	return "is not"
}

func (cp *Pair) StopEverything(pathToGpstopStateDir string) {
	logmsg := "Shutting down clusters. The old cluster %s running. The new cluster %s running."
	gplog.Info(fmt.Sprintf(logmsg, convert(cp.oldPostmasterRunning), convert(cp.newPostmasterRunning)))
	checklistManager := upgradestatus.NewChecklistManager(pathToGpstopStateDir)

	if cp.oldPostmasterRunning {
		cp.stopCluster(checklistManager, "gpstop.old", cp.oldBinDir, cp.oldMasterDataDirectory)
	}

	if cp.newPostmasterRunning {
		cp.stopCluster(checklistManager, "gpstop.new", cp.newBinDir, cp.newMasterDataDirectory)
	}
}

func (cp *Pair) EitherPostmasterRunning() bool {
	cp.oldPostmasterRunning = cp.postmasterRunning(cp.oldMasterDataDirectory)
	cp.newPostmasterRunning = cp.postmasterRunning(cp.newMasterDataDirectory)

	return cp.oldPostmasterRunning || cp.newPostmasterRunning
}

func (cp *Pair) postmasterRunning(masterDataDir string) bool {
	checkPid := "pgrep -F %s/postmaster.pid"

	checkPidCmd := cp.commandExecer("bash", "-c", fmt.Sprintf(checkPid, masterDataDir))
	err := checkPidCmd.Run()
	if err != nil {
		gplog.Error("Could not determine whether the cluster with MASTER_DATA_DIRECTORY: %s is running: %+v",
			masterDataDir, err)
		return false
	}
	return true
}

func (cp *Pair) stopCluster(stateManager *upgradestatus.ChecklistManager, step string, binDir string, masterDataDir string) {

	stateManager.ResetStateDir(step)

	gpstopShellArgs := fmt.Sprintf("source %s/../greenplum_path.sh; %s/gpstop -a -d %s",
		binDir, binDir, masterDataDir)
	stopCmd := cp.commandExecer("bash", "-c", gpstopShellArgs)
	gplog.Info("gpstop command: %+v", stopCmd)

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
