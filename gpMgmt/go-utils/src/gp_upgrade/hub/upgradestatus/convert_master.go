package upgradestatus

import (
	pb "gp_upgrade/idl"
	"gp_upgrade/utils"

	"bufio"
	"io"
	"os"
	"regexp"

	gpbackupUtils "github.com/greenplum-db/gp-common-go-libs/gplog"
)

type ConvertMaster struct {
	pgUpgradePath string
}

func NewConvertMaster(pgUpgradePath string) ConvertMaster {
	return ConvertMaster{pgUpgradePath: pgUpgradePath}
}

/*
 assumptions here are:
	- pg_upgrade will not fail without error before writing an inprogress file
	- when a new pg_upgrade is started it deletes all *.done and *.inprogress files
*/
func (c ConvertMaster) GetStatus() (*pb.UpgradeStepStatus, error) {
	var masterUpgradeStatus *pb.UpgradeStepStatus
	pgUpgradePath := c.pgUpgradePath

	if _, err := utils.System.Stat(pgUpgradePath); utils.System.IsNotExist(err) {
		gpbackupUtils.Info("setting status to PENDING")
		masterUpgradeStatus = &pb.UpgradeStepStatus{
			Step:   pb.UpgradeSteps_MASTERUPGRADE,
			Status: pb.StepStatus_PENDING,
		}
		return masterUpgradeStatus, nil
	}

	if pgUpgradeRunning() {
		gpbackupUtils.Info("setting status to RUNNING")
		masterUpgradeStatus = &pb.UpgradeStepStatus{
			Step:   pb.UpgradeSteps_MASTERUPGRADE,
			Status: pb.StepStatus_RUNNING,
		}
		return masterUpgradeStatus, nil
	}
	if !inProgressFilesExist(pgUpgradePath) && c.IsUpgradeComplete(pgUpgradePath) {
		gpbackupUtils.Info("setting status to COMPLETE")
		masterUpgradeStatus = &pb.UpgradeStepStatus{
			Step:   pb.UpgradeSteps_MASTERUPGRADE,
			Status: pb.StepStatus_COMPLETE,
		}
		return masterUpgradeStatus, nil
	}
	gpbackupUtils.Info("setting status to FAILED")
	masterUpgradeStatus = &pb.UpgradeStepStatus{
		Step:   pb.UpgradeSteps_MASTERUPGRADE,
		Status: pb.StepStatus_FAILED,
	}

	return masterUpgradeStatus, nil
}

func pgUpgradeRunning() bool {
	//if pgrep doesnt find target, ExecCmdOutput will return empty byte array and err.Error()="exit status 1"
	pgUpgradePids, err := utils.System.ExecCmdOutput("pgrep", "pg_upgrade")
	if err == nil && len(pgUpgradePids) != 0 {
		return true
	}
	return false
}

func inProgressFilesExist(pgUpgradePath string) bool {
	files, err := utils.System.FilePathGlob(pgUpgradePath + "/*.inprogress")
	if files == nil {
		return false
	}

	if err != nil {
		gpbackupUtils.Error("err is: ", err)
		return false
	}

	return true
}

func (c ConvertMaster) IsUpgradeComplete(pgUpgradePath string) bool {

	doneFiles, doneErr := utils.System.FilePathGlob(pgUpgradePath + "/*.done")
	if doneFiles == nil {
		return false
	}

	if doneErr != nil {
		gpbackupUtils.Error(doneErr.Error())
		return false
	}

	/* Get the latest done file
	 * Parse and find the "upgrade complete" and return true.
	 * otherwise, return false.
	 */

	latestDoneFile := doneFiles[0]
	fi, err := utils.System.Stat(latestDoneFile)
	if err != nil {
		gpbackupUtils.Error("IsUpgradeComplete: %v", err)
		return false
	}

	latestDoneFileModTime := fi.ModTime()
	for i := 1; i < len(doneFiles); i++ {
		doneFile := doneFiles[i]
		fi, err = os.Stat(doneFile)
		if err != nil {
			// TODO: What should we do here?
			continue
		}

		if fi.ModTime().After(latestDoneFileModTime) {
			latestDoneFile = doneFiles[i]
			latestDoneFileModTime = fi.ModTime()
		}
	}

	f, err := utils.System.Open(latestDoneFile)
	if err != nil {
		gpbackupUtils.Error(err.Error())
	}
	defer f.Close()
	r := bufio.NewReader(f)
	line, err := r.ReadString('\n')

	// TODO: Needs more error checking
	for err != io.EOF {
		if err != nil {
			gpbackupUtils.Error("IsUpgradeComplete: %v", err)
			return false
		}
		gpbackupUtils.Debug(line)
		re := regexp.MustCompile("Upgrade complete")
		if re.FindString(line) != "" {
			return true
		}

		line, err = r.ReadString('\n')
	}
	return false
}
