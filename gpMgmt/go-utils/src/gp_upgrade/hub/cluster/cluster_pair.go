package cluster

import (
	"fmt"

	"gp_upgrade/helpers"
	"gp_upgrade/hub/configutils"
	"gp_upgrade/hub/upgradestatus"
	"github.com/pkg/errors"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
)

type Pair struct {
	oldClusterReader       configutils.Reader
	newClusterReader       configutils.Reader
	OldMasterPort          int
	NewMasterPort          int
	OldMasterDataDirectory string
	NewMasterDataDirectory string
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

	oldConfReader := configutils.Reader{}
	oldConfReader.OfOldClusterConfig(baseDir)
	err = oldConfReader.Read()
	if err != nil {
		return fmt.Errorf("couldn't read old config file: %+v", err)
	}
	newConfReader := configutils.Reader{}
	newConfReader.OfNewClusterConfig(baseDir)
	err = newConfReader.Read()
	if err != nil {
		return fmt.Errorf("couldn't read new config file: %+v", err)
	}

	cp.OldMasterPort, cp.NewMasterPort, err = cp.GetMasterPorts()
	if err != nil {
		return err
	}

	cp.OldMasterDataDirectory, cp.NewMasterDataDirectory, err = cp.GetMasterDataDirs()
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
		cp.stopCluster(checklistManager, "gpstop.old", cp.oldBinDir, cp.OldMasterDataDirectory)
	}

	if cp.newPostmasterRunning {
		cp.stopCluster(checklistManager, "gpstop.new", cp.newBinDir, cp.NewMasterDataDirectory)
	}
}

func (cp *Pair) EitherPostmasterRunning() bool {
	cp.oldPostmasterRunning = cp.postmasterRunning(cp.OldMasterDataDirectory)
	cp.newPostmasterRunning = cp.postmasterRunning(cp.NewMasterDataDirectory)

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
	return cp.OldMasterPort, cp.NewMasterPort, cp.NewMasterDataDirectory
}

func (cp *Pair) GetMasterPorts() (int, int, error) {
	masterDbID := 1 // We are assuming that the master dbid will always be 1
	var oldMasterPort, newMasterPort int
	if cp.OldMasterPort != 0 {
		oldMasterPort = cp.OldMasterPort
	} else {
		oldMasterPort := cp.oldClusterReader.GetPortForSegment(masterDbID)
		if oldMasterPort == -1 {
			return -1, -1, errors.New("could not find port from old config")
		}
	}
	if cp.NewMasterPort != 0 {
		newMasterPort = cp.NewMasterPort
	} else {
		newMasterPort := cp.newClusterReader.GetPortForSegment(masterDbID)
		if newMasterPort == -1 {
			return -1, -1, errors.New("could not find port from new config")
		}
	}

	return oldMasterPort, newMasterPort, nil
}

func (cp *Pair) GetMasterDataDirs() (string, string, error) {
	var oldMasterDataDir, newMasterDataDir string
	if cp.OldMasterDataDirectory != "" {
		oldMasterDataDir = cp.OldMasterDataDirectory
	} else {
		oldMasterDataDir := cp.oldClusterReader.GetMasterDataDir()
		if oldMasterDataDir == "" {
			return "", "", errors.New("could not find old master data directory")
		}
	}
	if cp.NewMasterDataDirectory != "" {
		newMasterDataDir = cp.NewMasterDataDirectory
	} else {
		newMasterDataDir := cp.newClusterReader.GetMasterDataDir()
		if newMasterDataDir == "" {
			return "", "", errors.New("could not find new master data directory")
		}
	}
	return oldMasterDataDir, newMasterDataDir, nil
}
