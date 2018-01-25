package commanders_test

import (
	"errors"
	"gp_upgrade/cli/commanders"
	pb "gp_upgrade/idl"
	mockpb "gp_upgrade/mock_idl"

	"github.com/golang/mock/gomock"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/ginkgo/extensions/table"
	. "github.com/onsi/gomega"
)

var _ = Describe("Reporter", func() {

	var (
		spyClient *spyCliToHubClient
		spyLogger *spyLogger
		reporter  *commanders.Reporter
		client    *mockpb.MockCliToHubClient
		ctrl      *gomock.Controller
	)

	BeforeEach(func() {
		spyClient = newSpyCliToHubClient()
		spyLogger = newSpyLogger()
		reporter = commanders.NewReporter(spyClient, spyLogger)
		ctrl = gomock.NewController(GinkgoT())
		client = mockpb.NewMockCliToHubClient(ctrl)
	})

	AfterEach(func() {
		defer ctrl.Finish()
	})

	It("makes a call to StatusUpgrade", func() {
		err := reporter.OverallUpgradeStatus()
		Expect(err).ToNot(HaveOccurred())
		Expect(spyClient.statusUpgradeCount).To(Equal(1))
	})

	It("returns an error upon a failure", func() {
		spyClient.err = errors.New("some error")
		err := reporter.OverallUpgradeStatus()
		Expect(err).To(HaveOccurred())
	})

	It("sends all the right messages to the logger in the right order when reply contains multiple step-statuses", func() {
		spyClient.statusUpgradeReply = &pb.StatusUpgradeReply{
			ListOfUpgradeStepStatuses: []*pb.UpgradeStepStatus{
				{Step: pb.UpgradeSteps_PREPARE_INIT_CLUSTER, Status: pb.StepStatus_RUNNING},
				{Step: pb.UpgradeSteps_MASTERUPGRADE, Status: pb.StepStatus_PENDING},
			},
		}
		err := reporter.OverallUpgradeStatus()
		Expect(err).ToNot(HaveOccurred())
		Expect(spyLogger.infoCount).To(Equal(2))
		Expect(spyLogger.messages).To(Equal([]string{
			"RUNNING - Initialize upgrade target cluster",
			"PENDING - Run pg_upgrade on master",
		}))
	})

	DescribeTable("UpgradeStep Messages, basic cases where hub might return only one status",
		func(step pb.UpgradeSteps, status pb.StepStatus, expected string) {
			spyClient.statusUpgradeReply = &pb.StatusUpgradeReply{
				[]*pb.UpgradeStepStatus{
					{Step: step, Status: status},
				},
			}
			err := reporter.OverallUpgradeStatus()
			Expect(err).ToNot(HaveOccurred())
			Expect(spyLogger.messages).To(Equal([]string{
				expected,
			}))
		},
		Entry("unknown step", pb.UpgradeSteps_UNKNOWN_STEP, pb.StepStatus_PENDING, "PENDING - Unknown step"),
		Entry("configuration check", pb.UpgradeSteps_CHECK_CONFIG, pb.StepStatus_RUNNING, "RUNNING - Configuration Check"),
		Entry("install binaries on segments", pb.UpgradeSteps_SEGINSTALL, pb.StepStatus_COMPLETE, "COMPLETE - Install binaries on segments"),
		Entry("prepare init cluster", pb.UpgradeSteps_PREPARE_INIT_CLUSTER, pb.StepStatus_FAILED, "FAILED - Initialize upgrade target cluster"),
		Entry("upgrade on master", pb.UpgradeSteps_MASTERUPGRADE, pb.StepStatus_PENDING, "PENDING - Run pg_upgrade on master"),
		Entry("shutdown cluster", pb.UpgradeSteps_STOPPED_CLUSTER, pb.StepStatus_PENDING, "PENDING - Shutdown clusters"),
	)

})

type spyLogger struct {
	infoCount int
	messages  []string
}

func newSpyLogger() *spyLogger { return &spyLogger{} }

func (sl *spyLogger) Info(msg string, _ ...interface{}) {
	sl.infoCount++
	sl.messages = append(sl.messages, msg)
}
