package commanders_test

import (
	"gp_upgrade/cli/commanders"
	pb "gp_upgrade/idl"
	mockpb "gp_upgrade/mock_idl"

	"errors"

	"github.com/golang/mock/gomock"
	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ bool = Describe("object count tests", func() {

	var (
		client *mockpb.MockCliToHubClient
		ctrl   *gomock.Controller
	)

	BeforeEach(func() {
		ctrl = gomock.NewController(GinkgoT())
		client = mockpb.NewMockCliToHubClient(ctrl)
	})

	AfterEach(func() {
		defer ctrl.Finish()
	})
	Describe("Execute", func() {
		It("prints out version check is OK and that check version request was processed", func() {
			testStdout, _, _ := testhelper.SetupTestLogger()
			client.EXPECT().CheckVersion(
				gomock.Any(),
				&pb.CheckVersionRequest{DbPort: 9999, Host: "localhost"},
			).Return(&pb.CheckVersionReply{IsVersionCompatible: true}, nil)
			request := commanders.NewVersionChecker(client)
			err := request.Execute("localhost", 9999)
			Expect(err).To(BeNil())
			// this eventually should actually be an expect -- convert it
			Eventually(string(testStdout.Contents())).Should(ContainSubstring("gp_upgrade: Version Compatibility Check [OK]\n"))
			Eventually(string(testStdout.Contents())).Should(ContainSubstring("Check version request is processed."))
		})
		It("prints out version check failed and that check version request was processed", func() {
			testStdout, _, _ := testhelper.SetupTestLogger()
			client.EXPECT().CheckVersion(
				gomock.Any(),
				&pb.CheckVersionRequest{DbPort: 9999, Host: "localhost"},
			).Return(&pb.CheckVersionReply{IsVersionCompatible: false}, nil)
			request := commanders.NewVersionChecker(client)
			err := request.Execute("localhost", 9999)
			Expect(err).To(BeNil())
			// this eventually should actually be an expect -- convert it
			Eventually(string(testStdout.Contents())).Should(ContainSubstring("gp_upgrade: Version Compatibility Check [Failed]\n"))
			Eventually(string(testStdout.Contents())).Should(ContainSubstring("Check version request is processed."))
		})
		It("prints out that it was unable to connect to hub", func() {
			_, testStderr, _ := testhelper.SetupTestLogger()
			client.EXPECT().CheckVersion(
				gomock.Any(),
				&pb.CheckVersionRequest{DbPort: 9999, Host: "localhost"},
			).Return(&pb.CheckVersionReply{IsVersionCompatible: false}, errors.New("something went wrong"))
			request := commanders.NewVersionChecker(client)
			err := request.Execute("localhost", 9999)
			Expect(err).ToNot(BeNil())
			Expect(err.Error()).Should(ContainSubstring("something went wrong"))
			// this eventually should actually be an expect -- convert it
			Eventually(string(testStderr.Contents())).Should(ContainSubstring("ERROR - gRPC call to hub failed"))
		})
	})
})
