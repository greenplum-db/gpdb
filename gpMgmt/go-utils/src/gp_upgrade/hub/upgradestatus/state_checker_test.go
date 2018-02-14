package upgradestatus_test

import (
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"

	"gp_upgrade/hub/upgradestatus"
	pb "gp_upgrade/idl"
	"gp_upgrade/utils"

	"os"

	"path/filepath"

	"github.com/pkg/errors"
)

var _ = Describe("Upgradestatus/Seginstall", func() {
	AfterEach(func() {
		utils.System = utils.InitializeSystemFunctions()
	})
	It("Reports PENDING if no directory exists", func() {
		stateChecker := upgradestatus.NewStateCheck("/fake/path", pb.UpgradeSteps_SEGINSTALL)
		upgradeStepStatus, err := stateChecker.GetStatus()
		Expect(err).ToNot(HaveOccurred())
		Expect(upgradeStepStatus.Step).To(Equal(pb.UpgradeSteps_SEGINSTALL))
		Expect(upgradeStepStatus.Status).To(Equal(pb.StepStatus_PENDING))
	})
	It("Reports RUNNING if statedir exists and contains inprogress file", func() {
		fakePath := "/fake/path"
		utils.System.Stat = func(name string) (os.FileInfo, error) {
			if name == fakePath {
				return nil, nil
			}
			return nil, errors.New("unexpected Stat call")
		}
		utils.System.FilePathGlob = func(glob string) ([]string, error) {
			if glob == fakePath+"/*" {
				return []string{filepath.Join(fakePath, "in.progress")}, nil
			}
			return nil, errors.New("didn't match expected glob pattern")
		}
		stateChecker := upgradestatus.NewStateCheck(fakePath, pb.UpgradeSteps_SEGINSTALL)
		upgradeStepStatus, err := stateChecker.GetStatus()
		Expect(err).ToNot(HaveOccurred())
		Expect(upgradeStepStatus.Step).To(Equal(pb.UpgradeSteps_SEGINSTALL))
		Expect(upgradeStepStatus.Status).To(Equal(pb.StepStatus_RUNNING))
	})
	It("Reports FAILED if statedir exists and contains failed file", func() {
		fakePath := "/fake/path"
		utils.System.Stat = func(name string) (os.FileInfo, error) {
			if name == fakePath {
				return nil, nil
			}
			return nil, errors.New("unexpected Stat call")
		}
		utils.System.FilePathGlob = func(glob string) ([]string, error) {
			if glob == fakePath+"/*" {
				return []string{filepath.Join(fakePath, "failed")}, nil
			}
			return nil, errors.New("didn't match expected glob pattern")
		}
		stateChecker := upgradestatus.NewStateCheck(fakePath, pb.UpgradeSteps_SEGINSTALL)
		upgradeStepStatus, err := stateChecker.GetStatus()
		Expect(err).ToNot(HaveOccurred())
		Expect(upgradeStepStatus.Step).To(Equal(pb.UpgradeSteps_SEGINSTALL))
		Expect(upgradeStepStatus.Status).To(Equal(pb.StepStatus_FAILED))
	})

	It("errors if there is more than one file at the specified path", func() {
		overabundantDirectory := "/full/of/stuff"
		utils.System.Stat = func(name string) (os.FileInfo, error) {
			if name == overabundantDirectory {
				return nil, nil
			}
			return nil, errors.New("unexpected Stat call")
		}

		utils.System.FilePathGlob = func(glob string) ([]string, error) {
			if glob == overabundantDirectory+"/*" {
				return []string{"first", "second"}, nil
			}
			return nil, errors.New("didn't match expected glob pattern")
		}
		stateChecker := upgradestatus.NewStateCheck(overabundantDirectory, pb.UpgradeSteps_SEGINSTALL)
		upgradeStepStatus, err := stateChecker.GetStatus()

		Expect(err).To(HaveOccurred())
		Expect(upgradeStepStatus).To(BeNil())
	})
})
