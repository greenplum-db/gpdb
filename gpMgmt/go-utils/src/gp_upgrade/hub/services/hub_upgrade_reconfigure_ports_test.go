package services_test

import (
	"errors"
	"fmt"
	"io/ioutil"
	"os"

	"gp_upgrade/hub/cluster"
	"gp_upgrade/hub/services"
	pb "gp_upgrade/idl"
	"gp_upgrade/testutils"

	"google.golang.org/grpc"

	"gp_upgrade/utils"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("UpgradeReconfigurePorts", func() {
	var (
		reader        *testutils.SpyReader
		hub           *services.HubClient
		dir           string
		commandExecer *testutils.FakeCommandExecer
		errChan       chan error
		outChan       chan []byte
	)

	BeforeEach(func() {
		var err error
		dir, err = ioutil.TempDir("", "")
		Expect(err).ToNot(HaveOccurred())

		numInvocations := 0
		utils.System.ReadFile = func(filename string) ([]byte, error) {
			if numInvocations == 0 {
				numInvocations++
				return []byte(testutils.MASTER_ONLY_JSON), nil
			} else {
				return []byte(testutils.NEW_MASTER_JSON), nil
			}
		}
		reader = &testutils.SpyReader{
			Hostnames: []string{"hostone", "hosttwo"},
		}

		errChan = make(chan error, 2)
		outChan = make(chan []byte, 2)
		commandExecer = &testutils.FakeCommandExecer{}
		commandExecer.SetOutput(&testutils.FakeCommand{
			Err: errChan,
			Out: outChan,
		})
		clusterPair := &cluster.Pair{
			OldMasterPort:          25437,
			NewMasterPort:          35437,
			OldMasterDataDirectory: "/old/datadir",
			NewMasterDataDirectory: "/new/datadir",
		}
		hub = services.NewHub(clusterPair, reader, grpc.DialContext, commandExecer.Exec, &services.HubConfig{
			StateDir: dir,
		})
	})

	AfterEach(func() {
		utils.System = utils.InitializeSystemFunctions()
		os.RemoveAll(dir)
	})

	It("reconfigures port in postgresql.conf on master", func() {
		reply, err := hub.UpgradeReconfigurePorts(nil, &pb.UpgradeReconfigurePortsRequest{})
		Expect(reply).To(Equal(&pb.UpgradeReconfigurePortsReply{}))
		Expect(err).To(BeNil())
		Expect(commandExecer.Calls()[0]).To(ContainSubstring(fmt.Sprintf(services.SedAndMvString, 35437, 25437, "/new/datadir")))
	})

	It("returns err is reconfigure cmd fails", func() {
		errChan <- errors.New("boom")
		reply, err := hub.UpgradeReconfigurePorts(nil, &pb.UpgradeReconfigurePortsRequest{})
		Expect(reply).To(BeNil())
		Expect(err).ToNot(BeNil())
		Expect(commandExecer.Calls()[0]).To(ContainSubstring(fmt.Sprintf(services.SedAndMvString, 35437, 25437, "/new/datadir")))
	})

})
