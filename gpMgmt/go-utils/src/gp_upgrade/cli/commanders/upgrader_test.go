package commanders_test

import (
	"errors"

	"gp_upgrade/cli/commanders"
	pb "gp_upgrade/idl"
	mockpb "gp_upgrade/mock_idl"
	"gp_upgrade/testutils"

	"github.com/golang/mock/gomock"
	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"github.com/onsi/gomega/gbytes"
	"gp_upgrade/utils"
)

var _ = Describe("reporter", func() {
	var (
		client *mockpb.MockCliToHubClient
		ctrl   *gomock.Controller

		hubClient  *testutils.MockHubClient
		upgrader   *commanders.Upgrader
		testStdout *gbytes.Buffer
		testStderr *gbytes.Buffer
	)

	BeforeEach(func() {
		testStdout, testStderr, _ = testhelper.SetupTestLogger()

		ctrl = gomock.NewController(GinkgoT())
		client = mockpb.NewMockCliToHubClient(ctrl)

		hubClient = testutils.NewMockHubClient()
		upgrader = commanders.NewUpgrader(hubClient)
	})

	AfterEach(func() {
		utils.System = utils.InitializeSystemFunctions()
		defer ctrl.Finish()
	})

	Describe("ConvertMaster", func() {
		It("Reports success when pg_upgrade started", func() {
			client.EXPECT().UpgradeConvertMaster(
				gomock.Any(),
				&pb.UpgradeConvertMasterRequest{},
			).Return(&pb.UpgradeConvertMasterReply{}, nil)
			err := commanders.NewUpgrader(client).ConvertMaster("", "", "", "")
			Expect(err).To(BeNil())
			Eventually(testStdout).Should(gbytes.Say("Kicked off pg_upgrade request"))
		})

		It("reports failure when command fails to connect to the hub", func() {
			client.EXPECT().UpgradeConvertMaster(
				gomock.Any(),
				&pb.UpgradeConvertMasterRequest{},
			).Return(&pb.UpgradeConvertMasterReply{}, errors.New("something bad happened"))
			err := commanders.NewUpgrader(client).ConvertMaster("", "", "", "")
			Expect(err).ToNot(BeNil())
			Eventually(testStderr).Should(gbytes.Say("ERROR - Unable to connect to hub"))

		})
	})

	Describe("ConvertPrimaries", func() {
		It("returns no error when the hub returns no error", func() {
			err := upgrader.ConvertPrimaries("/old/bin", "/new/bin")
			Expect(err).ToNot(HaveOccurred())

			Expect(hubClient.UpgradeConvertPrimariesRequest).To(Equal(&pb.UpgradeConvertPrimariesRequest{
				OldBinDir: "/old/bin",
				NewBinDir: "/new/bin",
			}))
		})

		It("returns an error when the hub returns an error", func() {
			hubClient.Err = errors.New("hub error")

			err := upgrader.ConvertPrimaries("", "")
			Expect(err).To(HaveOccurred())
		})
	})

	Describe("ShareOids", func() {
		It("returns no error when oids are shared successfully", func() {
			err := upgrader.ShareOids()
			Expect(err).ToNot(HaveOccurred())

			Expect(hubClient.UpgradeShareOidsRequest).To(Equal(&pb.UpgradeShareOidsRequest{}))
		})

		It("returns an error when oids cannot be shared", func() {
			hubClient.Err = errors.New("test share oids failed")

			err := upgrader.ShareOids()
			Expect(err).To(HaveOccurred())
		})
	})

	Describe("ReconfigurePorts", func() {
		It("returns nil error when ports are reconfigured successfully", func() {
			err := upgrader.ReconfigurePorts()
			Expect(err).ToNot(HaveOccurred())

			Expect(hubClient.UpgradeReconfigurePortsRequest).To(Equal(&pb.UpgradeReconfigurePortsRequest{}))
		})

		It("returns error when ports cannot be reconfigured", func() {
			hubClient.Err = errors.New("reconfigure ports failed")

			err := upgrader.ReconfigurePorts()
			Expect(err).To(HaveOccurred())
		})
	})
})
