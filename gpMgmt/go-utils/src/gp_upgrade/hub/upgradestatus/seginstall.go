package upgradestatus

import (
	pb "gp_upgrade/idl"
	"gp_upgrade/utils"
	"path/filepath"

	"github.com/pkg/errors"
)

//upgradestatus/Seginstall represents the necessary information and functions
// to determine the status of the seginstall step
//
// seginstallPath is expected to be an absolute path
type Seginstall struct {
	seginstallPath string
}

func NewSeginstall(seginstallPath string) Seginstall {
	return Seginstall{
		seginstallPath: seginstallPath,
	}
}

func (c Seginstall) GetStatus() (*pb.UpgradeStepStatus, error) {
	_, err := utils.System.Stat(c.seginstallPath)
	if err != nil {
		return &pb.UpgradeStepStatus{Step: pb.UpgradeSteps_SEGINSTALL, Status: pb.StepStatus_PENDING}, nil
	}
	files, err := utils.System.FilePathGlob(filepath.Join(c.seginstallPath, "*"))
	if len(files) > 1 {
		return nil, errors.New("got more files than expected")
	} else if len(files) == 1 {
		switch files[0] {
		case filepath.Join(c.seginstallPath, "failed"):
			return &pb.UpgradeStepStatus{Step: pb.UpgradeSteps_SEGINSTALL, Status: pb.StepStatus_FAILED}, nil
		case filepath.Join(c.seginstallPath, "completed"):
			return &pb.UpgradeStepStatus{Step: pb.UpgradeSteps_SEGINSTALL, Status: pb.StepStatus_COMPLETE}, nil
		case filepath.Join(c.seginstallPath, "in.progress"):
			return &pb.UpgradeStepStatus{Step: pb.UpgradeSteps_SEGINSTALL, Status: pb.StepStatus_RUNNING}, nil
		}
	}

	return &pb.UpgradeStepStatus{Step: pb.UpgradeSteps_SEGINSTALL, Status: pb.StepStatus_PENDING}, nil
}
