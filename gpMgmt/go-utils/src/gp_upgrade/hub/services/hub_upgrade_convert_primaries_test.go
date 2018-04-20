package services_test

import (
	"errors"
	"io/ioutil"

	"gp_upgrade/hub/configutils"
	"gp_upgrade/hub/services"
	pb "gp_upgrade/idl"
	"gp_upgrade/testutils"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"google.golang.org/grpc"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"gp_upgrade/utils"
)

var _ = Describe("hub.UpgradeConvertPrimaries()", func() {
	var (
		dir           string
		commandExecer *testutils.FakeCommandExecer
		reader        *testutils.SpyReader
		hub           *services.HubClient
		mockAgent     *testutils.MockAgentServer
		segmentConfs  chan configutils.SegmentConfiguration
		port          int
		request       *pb.UpgradeConvertPrimariesRequest
		oldConf       configutils.SegmentConfiguration
		newConf       configutils.SegmentConfiguration
	)

	BeforeEach(func() {
		testhelper.SetupTestLogger()

		segmentConfs = make(chan configutils.SegmentConfiguration, 2)
		reader = &testutils.SpyReader{
			Hostnames:             []string{"localhost", "localhost"},
			SegmentConfigurations: segmentConfs,
		}

		mockAgent, port = testutils.NewMockAgentServer()

		var err error
		dir, err = ioutil.TempDir("", "")
		Expect(err).ToNot(HaveOccurred())
		conf := &services.HubConfig{
			StateDir:       dir,
			HubToAgentPort: port,
		}

		oldConf = configutils.SegmentConfiguration{
			newSegment(0, "localhost", "p", "old/datadir1", 1),
			newSegment(1, "localhost", "p", "old/datadir2", 2),
			newSegment(1, "localhost", "m", "old/datadir2/mirror", 3),
			newSegment(-1, "localhost", "p", "old/master", 4),
		}

		newConf = configutils.SegmentConfiguration{
			newSegment(0, "localhost", "p", "new/datadir1", 11),
			newSegment(1, "localhost", "p", "new/datadir2", 22),
			newSegment(1, "localhost", "m", "new/datadir2/mirror", 33),
			newSegment(-1, "localhost", "p", "new/master", 44),
		}

		request = &pb.UpgradeConvertPrimariesRequest{
			OldBinDir: "/old/bin",
			NewBinDir: "/new/bin",
		}

		commandExecer = &testutils.FakeCommandExecer{}
		commandExecer.SetOutput(&testutils.FakeCommand{})

		hub = services.NewHub(nil, reader, grpc.DialContext, commandExecer.Exec, conf)
	})
	AfterEach(func() {
		utils.System = utils.InitializeSystemFunctions()
		defer mockAgent.Stop()
	})

	It("returns nil error, and agent receives only expected segmentConfig values", func() {

		segmentConfs <- oldConf

		segmentConfs <- newConf

		_, err := hub.UpgradeConvertPrimaries(nil, request)
		Expect(err).ToNot(HaveOccurred())

		Expect(mockAgent.UpgradeConvertPrimarySegmentsRequest.OldBinDir).To(Equal("/old/bin"))
		Expect(mockAgent.UpgradeConvertPrimarySegmentsRequest.NewBinDir).To(Equal("/new/bin"))
		Expect(mockAgent.UpgradeConvertPrimarySegmentsRequest.DataDirPairs).To(ConsistOf([]*pb.DataDirPair{
			{OldDataDir: "old/datadir1", NewDataDir: "new/datadir1", Content: 0, OldPort: 1, NewPort: 11},
			{OldDataDir: "old/datadir2", NewDataDir: "new/datadir2", Content: 1, OldPort: 2, NewPort: 22},
		}))
	})

	It("returns an error if new config does not contain all the same content as the old config", func() {
		segmentConfs <- oldConf

		segmentConfs <- configutils.SegmentConfiguration{
			newSegment(0, "localhost", "p", "new/datadir1", 11),
		}

		_, err := hub.UpgradeConvertPrimaries(nil, request)
		Expect(err).To(HaveOccurred())
		Expect(mockAgent.NumberOfCalls()).To(Equal(0))
	})

	It("returns an error if the content matches, but the hostname does not", func() {
		segmentConfs <- oldConf

		newConf[0].Hostname = "localhost2"
		segmentConfs <- newConf

		_, err := hub.UpgradeConvertPrimaries(nil, request)
		Expect(err).To(HaveOccurred())

		Expect(mockAgent.NumberOfCalls()).To(Equal(0))
	})

	It("returns no error if segmentConfigs are nil", func() {
		segmentConfs <- nil
		segmentConfs <- nil

		_, err := hub.UpgradeConvertPrimaries(nil, request)
		Expect(err).ToNot(HaveOccurred())
	})

	It("returns an error if any upgrade primary call to any agent fails", func() {
		mockAgent.Err <- errors.New("fail upgrade primary call")

		_, err := hub.UpgradeConvertPrimaries(nil, request)
		Expect(err).To(HaveOccurred())

		Expect(mockAgent.NumberOfCalls()).To(Equal(2))
	})

	It("returns an error if the agent is inaccessible", func() {
		mockAgent.Stop()

		_, err := hub.UpgradeConvertPrimaries(nil, request)
		Expect(err).To(HaveOccurred())

		Expect(mockAgent.NumberOfCalls()).To(Equal(0))
	})
})

func newSegment(content int, hostname, preferredRole, dataDir string, port int) configutils.Segment {
	return configutils.Segment{
		Content:       content,
		Hostname:      hostname,
		PreferredRole: preferredRole,
		Datadir:       dataDir,
		Port:          port,
	}
}
