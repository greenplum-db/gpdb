package services

import (
	"path/filepath"
	"strings"

	"gp_upgrade/hub/configutils"
	"gp_upgrade/hub/upgradestatus"
	pb "gp_upgrade/idl"
	"gp_upgrade/utils"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
	"golang.org/x/net/context"
)

func (h *HubClient) StatusUpgrade(ctx context.Context, in *pb.StatusUpgradeRequest) (*pb.StatusUpgradeReply, error) {
	gplog.Info("starting StatusUpgrade")

	checkConfigStatePath := filepath.Join(h.conf.StateDir, "cluster_config.json")
	gplog.Debug("looking for check-config.json at %s", checkConfigStatePath)

	checkconfigStatus := &pb.UpgradeStepStatus{
		Step: pb.UpgradeSteps_CHECK_CONFIG,
	}
	if _, err := utils.System.Stat(checkConfigStatePath); utils.System.IsNotExist(err) {
		checkconfigStatus.Status = pb.StepStatus_PENDING
	} else {
		checkconfigStatus.Status = pb.StepStatus_COMPLETE
	}

	prepareInitStatus, _ := GetPrepareNewClusterConfigStatus(h.conf.StateDir)

	seginstallStatePath := filepath.Join(h.conf.StateDir, "seginstall")
	gplog.Debug("looking for seginstall State at %s", seginstallStatePath)
	seginstallState := upgradestatus.NewStateCheck(seginstallStatePath, pb.UpgradeSteps_SEGINSTALL)
	seginstallStatus, _ := seginstallState.GetStatus()

	gpstopStatePath := filepath.Join(h.conf.StateDir, "gpstop")
	clusterPair := upgradestatus.NewShutDownClusters(gpstopStatePath, h.commandExecer)
	shutdownClustersStatus, _ := clusterPair.GetStatus()

	pgUpgradePath := filepath.Join(h.conf.StateDir, "pg_upgrade")
	convertMaster := upgradestatus.NewPGUpgradeStatusChecker(pgUpgradePath, h.configreader.GetMasterDataDir(), h.commandExecer)
	masterUpgradeStatus, _ := convertMaster.GetStatus()

	startAgentsStatePath := filepath.Join(h.conf.StateDir, "start-agents")
	prepareStartAgentsState := upgradestatus.NewStateCheck(startAgentsStatePath, pb.UpgradeSteps_PREPARE_START_AGENTS)
	startAgentsStatus, _ := prepareStartAgentsState.GetStatus()

	shareOidsPath := filepath.Join(h.conf.StateDir, "share-oids")
	shareOidsState := upgradestatus.NewStateCheck(shareOidsPath, pb.UpgradeSteps_SHARE_OIDS)
	shareOidsStatus, _ := shareOidsState.GetStatus()

	validateStartClusterPath := filepath.Join(h.conf.StateDir, "validate-start-cluster")
	validateStartClusterState := upgradestatus.NewStateCheck(validateStartClusterPath, pb.UpgradeSteps_VALIDATE_START_CLUSTER)
	validateStartClusterStatus, _ := validateStartClusterState.GetStatus()

	conversionStatus, _ := h.StatusConversion(nil, &pb.StatusConversionRequest{})
	upgradeConvertPrimariesStatus := &pb.UpgradeStepStatus{
		Step: pb.UpgradeSteps_CONVERT_PRIMARIES,
	}

	reconfigurePortsPath := filepath.Join(h.conf.StateDir, "reconfigure-ports")
	reconfigurePortsState := upgradestatus.NewStateCheck(reconfigurePortsPath, pb.UpgradeSteps_RECONFIGURE_PORTS)
	reconfigurePortsStatus, _ := reconfigurePortsState.GetStatus()

	statuses := strings.Join(conversionStatus.GetConversionStatuses(), " ")
	if strings.Contains(statuses, "FAILED") {
		upgradeConvertPrimariesStatus.Status = pb.StepStatus_FAILED
	} else if strings.Contains(statuses, "RUNNING") {
		upgradeConvertPrimariesStatus.Status = pb.StepStatus_RUNNING
	} else if strings.Contains(statuses, "COMPLETE") {
		upgradeConvertPrimariesStatus.Status = pb.StepStatus_COMPLETE
	} else {
		upgradeConvertPrimariesStatus.Status = pb.StepStatus_PENDING
	}

	return &pb.StatusUpgradeReply{
		ListOfUpgradeStepStatuses: []*pb.UpgradeStepStatus{
			checkconfigStatus,
			seginstallStatus,
			prepareInitStatus,
			shutdownClustersStatus,
			masterUpgradeStatus,
			startAgentsStatus,
			shareOidsStatus,
			validateStartClusterStatus,
			upgradeConvertPrimariesStatus,
			reconfigurePortsStatus,
		},
	}, nil
}

func GetPrepareNewClusterConfigStatus(base string) (*pb.UpgradeStepStatus, error) {
	/* Treat all stat failures as cannot find file. Conceal worse failures atm.*/
	_, err := utils.System.Stat(configutils.GetNewClusterConfigFilePath(base))

	if err != nil {
		gplog.Debug("%v", err)
		return &pb.UpgradeStepStatus{
			Step:   pb.UpgradeSteps_PREPARE_INIT_CLUSTER,
			Status: pb.StepStatus_PENDING,
		}, nil
	}

	return &pb.UpgradeStepStatus{
		Step:   pb.UpgradeSteps_PREPARE_INIT_CLUSTER,
		Status: pb.StepStatus_COMPLETE,
	}, nil
}
