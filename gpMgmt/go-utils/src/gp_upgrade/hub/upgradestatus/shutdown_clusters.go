package upgradestatus

import (
	pb "gp_upgrade/idl"
	"gp_upgrade/utils"

	gpbackupUtils "github.com/greenplum-db/gp-common-go-libs/gplog"
)

type ShutDownClusters struct {
	gpstopStatePath string
}

func NewShutDownClusters(gpstopStatePath string) ShutDownClusters {
	return ShutDownClusters{gpstopStatePath: gpstopStatePath}
}

/*
 assumptions here are:
	- gpstop will not fail without error before writing an inprogress file
	- when a new gpstop is started it deletes all *.done and *.inprogress files
*/
func (s ShutDownClusters) GetStatus() (*pb.UpgradeStepStatus, error) {
	var shutdownClustersStatus *pb.UpgradeStepStatus
	gpstopStatePath := s.gpstopStatePath

	if _, err := utils.System.Stat(gpstopStatePath); utils.System.IsNotExist(err) {
		gpbackupUtils.Info("setting status to PENDING")
		shutdownClustersStatus = &pb.UpgradeStepStatus{
			Step:   pb.UpgradeSteps_STOPPED_CLUSTER,
			Status: pb.StepStatus_PENDING,
		}
		return shutdownClustersStatus, nil
	}

	/* There can be cases where gpstop is running but not as part of the pre-setup
	 * in which case, we shouldn't be detecting that as a running state.
	 * We only care if the inprogress file exists. We are relying on the hub to never go down
	 * for this state processing to work.
	 */
	if isGpstopRunning() && s.inProgressFilesExist(gpstopStatePath) {
		gpbackupUtils.Info("setting status to RUNNING")
		shutdownClustersStatus = &pb.UpgradeStepStatus{
			Step:   pb.UpgradeSteps_STOPPED_CLUSTER,
			Status: pb.StepStatus_RUNNING,
		}
		return shutdownClustersStatus, nil
	}

	if !s.inProgressFilesExist(gpstopStatePath) && s.IsStopComplete(gpstopStatePath) {
		gpbackupUtils.Info("setting status to COMPLETE")
		shutdownClustersStatus = &pb.UpgradeStepStatus{
			Step:   pb.UpgradeSteps_STOPPED_CLUSTER,
			Status: pb.StepStatus_COMPLETE,
		}
		return shutdownClustersStatus, nil
	}

	gpbackupUtils.Info("setting status to FAILED")
	shutdownClustersStatus = &pb.UpgradeStepStatus{
		Step:   pb.UpgradeSteps_STOPPED_CLUSTER,
		Status: pb.StepStatus_FAILED,
	}

	return shutdownClustersStatus, nil
}

func isGpstopRunning() bool {
	//if pgrep doesnt find target, ExecCmdOutput will return empty byte array and err.Error()="exit status 1"
	pgUpgradePids, err := utils.System.ExecCmdOutput("pgrep", "-f", "gpstop")
	if err == nil && len(pgUpgradePids) != 0 {
		return true
	}
	return false
}

func (s ShutDownClusters) inProgressFilesExist(gpstopStatePath string) bool {
	files, err := utils.System.FilePathGlob(gpstopStatePath + "/*.inprogress")
	if files == nil {
		return false
	}

	if err != nil {
		gpbackupUtils.Error("err is: ", err)
		return false
	}

	return true
}

func (s ShutDownClusters) IsStopComplete(gpstopStatePath string) bool {

	completeFiles, completeErr := utils.System.FilePathGlob(gpstopStatePath + "/*.complete")
	if completeFiles == nil {
		return false
	}

	if completeErr != nil {
		gpbackupUtils.Error(completeErr.Error())
		return false
	}

	/* There should only be two completed files.
	 * One for gpstop.old and one for gpstop.new
	 */
	if len(completeFiles) == 2 {
		return true
	}

	return false
}
