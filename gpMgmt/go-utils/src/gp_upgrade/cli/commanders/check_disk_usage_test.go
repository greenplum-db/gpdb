package commanders_test

import (
	"gp_upgrade/cli/commanders"
	pb "gp_upgrade/idl"
	mockpb "gp_upgrade/mock_idl"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/greenplum-db/gpbackup/testutils"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"github.com/pkg/errors"
)

var _ = Describe("object count tests", func() {

	var (
		client *mockpb.MockCliToHubClient
		t      *testing.T
		ctrl   *gomock.Controller
	)

	BeforeEach(func() {
		ctrl = gomock.NewController(t)
		client = mockpb.NewMockCliToHubClient(ctrl)
	})

	AfterEach(func() {
		defer ctrl.Finish()
	})
	Describe("Execute", func() {
		It("logs and returns error if connection to hub fails", func() {
			_, _, _, testLogFile := testutils.SetupTestLogger()

			client.EXPECT().CheckDiskUsage(
				gomock.Any(),
				&pb.CheckDiskUsageRequest{},
			).Return(&pb.CheckDiskUsageReply{}, errors.New("couldn't connect to hub"))

			request := commanders.NewDiskUsageChecker(client)
			err := request.Execute(9999)

			Expect(err).ToNot(BeNil())
			Expect(string(testLogFile.Contents())).To(ContainSubstring("ERROR - gRPC call to hub failed"))
		})
		It("prints out the results of disk usage check from gRPC reply", func() {
			_, testStdout, _, _ := testutils.SetupTestLogger()

			var expectedFilesystemsUsage []string
			expectedFilesystemsUsage = append(expectedFilesystemsUsage, "diskspace check - hostC  - Couldn't connect")
			expectedFilesystemsUsage = append(expectedFilesystemsUsage, "diskspace check - hostA  - OK")
			//to log a percent sign, use %% to avoid logger substitution
			expectedFilesystemsUsage = append(expectedFilesystemsUsage, "diskspace check - hostD  - WARNING /data 90%% use")

			client.EXPECT().CheckDiskUsage(
				gomock.Any(),
				&pb.CheckDiskUsageRequest{},
			).Return(&pb.CheckDiskUsageReply{SegmentFileSysUsage: expectedFilesystemsUsage}, nil)

			request := commanders.NewDiskUsageChecker(client)
			err := request.Execute(9999)

			Expect(err).To(BeNil())
			Expect(string(testStdout.Contents())).To(ContainSubstring("diskspace check - hostC  - Couldn't connect"))
			Expect(string(testStdout.Contents())).To(ContainSubstring("diskspace check - hostA  - OK"))
			Expect(string(testStdout.Contents())).To(ContainSubstring("diskspace check - hostD  - WARNING /data 90% use"))

		})
	})
})
