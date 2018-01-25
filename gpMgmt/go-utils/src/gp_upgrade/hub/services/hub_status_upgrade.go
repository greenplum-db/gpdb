package services

import (
	"errors"
	"gp_upgrade/hub/configutils"
	"gp_upgrade/hub/upgradestatus"
	pb "gp_upgrade/idl"
	"gp_upgrade/utils"
	"os"
	"path/filepath"

	gpbackupUtils "github.com/greenplum-db/gpbackup/utils"
	"golang.org/x/net/context"
)

func (s *CatchAllCliToHubListenerImpl) StatusUpgrade(ctx context.Context, in *pb.StatusUpgradeRequest) (*pb.StatusUpgradeReply, error) {
	gpbackupUtils.GetLogger().Info("starting StatusUpgrade")
	demoStepStatus := &pb.UpgradeStepStatus{
		Step:   pb.UpgradeSteps_CHECK_CONFIG,
		Status: pb.StepStatus_PENDING,
	}

	prepareInitStatus, _ := GetPrepareNewClusterConfigStatus()

	homeDirectory := os.Getenv("HOME")
	if homeDirectory == "" {
		return nil, errors.New("Could not find the HOME environment")
	}

	seginstallStatus := &pb.UpgradeStepStatus{
		Step:   pb.UpgradeSteps_SEGINSTALL,
		Status: pb.StepStatus_PENDING,
	}

	gpstopStatePath := filepath.Join(homeDirectory, ".gp_upgrade/gpstop")
	clusterPair := upgradestatus.NewShutDownClusters(gpstopStatePath)

	pgUpgradePath := filepath.Join(homeDirectory, ".gp_upgrade/pg_upgrade")
	convertMaster := upgradestatus.NewConvertMaster(pgUpgradePath)

	shutdownClustersStatus, _ := clusterPair.GetStatus()
	masterUpgradeStatus, _ := convertMaster.GetStatus()

	reply := &pb.StatusUpgradeReply{}
	reply.ListOfUpgradeStepStatuses = append(reply.ListOfUpgradeStepStatuses, demoStepStatus, seginstallStatus, prepareInitStatus, shutdownClustersStatus, masterUpgradeStatus)
	return reply, nil
}

func GetPrepareNewClusterConfigStatus() (*pb.UpgradeStepStatus, error) {
	/* Treat all stat failures as cannot find file. Conceal worse failures atm.*/
	_, err := utils.System.Stat(configutils.GetNewClusterConfigFilePath())

	if err != nil {
		gpbackupUtils.GetLogger().Debug("%v", err)
		return &pb.UpgradeStepStatus{Step: pb.UpgradeSteps_PREPARE_INIT_CLUSTER,
			Status: pb.StepStatus_PENDING}, nil
	}

	return &pb.UpgradeStepStatus{Step: pb.UpgradeSteps_PREPARE_INIT_CLUSTER,
		Status: pb.StepStatus_COMPLETE}, nil
}
