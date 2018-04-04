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
)

var _ = Describe("reporter", func() {
	var (
		client *mockpb.MockCliToHubClient
		ctrl   *gomock.Controller

		hubClient *testutils.MockHubClient
		upgrader  *commanders.Upgrader
	)

	BeforeEach(func() {
		ctrl = gomock.NewController(GinkgoT())
		client = mockpb.NewMockCliToHubClient(ctrl)

		hubClient = testutils.NewMockHubClient()
		upgrader = commanders.NewUpgrader(hubClient)
	})

	AfterEach(func() {
		defer ctrl.Finish()
	})

	Describe("ConvertMaster", func() {
		It("Reports success when pg_upgrade started", func() {
			testStdout, _, _ := testhelper.SetupTestLogger()
			client.EXPECT().UpgradeConvertMaster(
				gomock.Any(),
				&pb.UpgradeConvertMasterRequest{},
			).Return(&pb.UpgradeConvertMasterReply{}, nil)
			err := commanders.NewUpgrader(client).ConvertMaster("", "", "", "")
			Expect(err).To(BeNil())
			Eventually(testStdout).Should(gbytes.Say("Kicked off pg_upgrade request"))
		})

		It("reports failure when command fails to connect to the hub", func() {
			_, testStderr, _ := testhelper.SetupTestLogger()
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
			testhelper.SetupTestLogger()

			err := commanders.NewUpgrader(hubClient).ConvertPrimaries("/old/bin", "/new/bin")
			Expect(err).ToNot(HaveOccurred())

			Expect(hubClient.UpgradeConvertPrimariesRequest).To(Equal(&pb.UpgradeConvertPrimariesRequest{
				OldBinDir: "/old/bin",
				NewBinDir: "/new/bin",
			}))
		})

		It("returns an error when the hub returns an error", func() {
			testhelper.SetupTestLogger()

			hubClient.Err = errors.New("hub error")

			err := commanders.NewUpgrader(hubClient).ConvertPrimaries("", "")
			Expect(err).To(HaveOccurred())
		})
	})
})
