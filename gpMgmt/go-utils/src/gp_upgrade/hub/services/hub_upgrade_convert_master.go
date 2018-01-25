package services

import (
	"errors"
	"fmt"
	"gp_upgrade/hub/configutils"
	pb "gp_upgrade/idl"
	"gp_upgrade/utils"

	"os"
	"path/filepath"

	gpbackupUtils "github.com/greenplum-db/gpbackup/utils"
	"golang.org/x/net/context"
)

var (
	GetMasterDataDirs = getMasterDataDirs
)

func (s *CatchAllCliToHubListenerImpl) UpgradeConvertMaster(ctx context.Context,
	in *pb.UpgradeConvertMasterRequest) (*pb.UpgradeConvertMasterReply, error) {

	gpbackupUtils.GetLogger().Info("Starting master upgrade")
	//need to remember where we ran, i.e. pathToUpgradeWD, b/c pg_upgrade generates some files that need to be copied to QE nodes later
	//this is also where the 1.done, 2.inprogress ... files will be written
	homeDirectory := os.Getenv("HOME")
	if homeDirectory == "" {
		return nil, errors.New("Could not find the home directory environemnt variable")

	}
	gpUpgradeDirectory := homeDirectory + "/.gp_upgrade"
	err := ConvertMaster(gpUpgradeDirectory+"/pg_upgrade", in.OldBinDir, in.NewBinDir)
	if err != nil {
		gpbackupUtils.GetLogger().Error("%v", err)
		return nil, err
	}
	return &pb.UpgradeConvertMasterReply{}, nil
}

func ConvertMaster(pathToUpgradeWD string, oldBinDir string, newBinDir string) error {
	err := os.Mkdir(pathToUpgradeWD, 0700)
	if err != nil {
		gpbackupUtils.GetLogger().Error("mkdir %s failed: %v. Is there an pg_upgrade in progress?", pathToUpgradeWD, err)
	}

	pgUpgradeLog := filepath.Join(pathToUpgradeWD, "/pg_upgrade_master.log")
	f, _ := os.Create(pgUpgradeLog) /* We already made sure above that we have a prestine directory */

	oldMasterDataDir, newMasterDataDir, err := GetMasterDataDirs() // TODO: this will need to the appropriate location
	if err != nil {
		return err
	}

	upgradeCmdArgs := fmt.Sprintf("unset PGHOST; unset PGPORT; cd %s && nohup %s --old-bindir=%s --old-datadir=%s --new-bindir=%s --new-datadir=%s --dispatcher-mode --progress",
		pathToUpgradeWD, newBinDir+"/pg_upgrade", oldBinDir, oldMasterDataDir, newBinDir, newMasterDataDir)

	//export ENV VARS instead of passing on cmd line?
	upgradeCommand := utils.System.ExecCommand("bash", "-c", upgradeCmdArgs)

	// redirect both stdout and stderr to the log file
	upgradeCommand.Stdout = f
	upgradeCommand.Stderr = f

	//TODO check the rc on this? keep a pid?
	err = upgradeCommand.Start()
	if err != nil {
		gpbackupUtils.GetLogger().Error("An error occured: %v", err)
		return err
	}
	gpbackupUtils.GetLogger().Info("Upgrade command: %v", upgradeCommand)
	gpbackupUtils.GetLogger().Info("Found no errors when starting the upgrade")

	return nil
}

func getMasterDataDirs() (string, string, error) {
	var err error
	reader := configutils.Reader{}
	reader.OfOldClusterConfig()
	err = reader.Read()
	if err != nil {
		gpbackupUtils.GetLogger().Error("Unable to read the file: %v", err)
		return "", "", err
	}

	oldMasterDataDir := reader.GetMasterDataDir()
	if oldMasterDataDir == "" {
		return "", "", errors.New("could not find old master data directory")
	}

	reader = configutils.Reader{}
	reader.OfNewClusterConfig()
	err = reader.Read()
	if err != nil {
		gpbackupUtils.GetLogger().Error("Unable to read the file: %v", err)
		return "", "", err
	}

	newMasterDataDir := reader.GetMasterDataDir()
	if oldMasterDataDir == "" {
		return "", "", errors.New("could not find old master data directory")
	}

	return oldMasterDataDir, newMasterDataDir, err
}
