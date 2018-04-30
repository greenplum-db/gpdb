package integrations_test

import (
	"errors"
	"io/ioutil"
	"os"

	"gp_upgrade/hub/cluster"
	"gp_upgrade/hub/configutils"
	hubServices "gp_upgrade/hub/services"
	"gp_upgrade/testutils"

	"google.golang.org/grpc"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gexec"
)

var _ = Describe("upgrade reconfigure ports", func() {

	var (
		dir       string
		hub       *hubServices.HubClient
		hubExecer *testutils.FakeCommandExecer
		agentPort int

		outChan chan []byte
		errChan chan error
	)

	BeforeEach(func() {
		var err error
		dir, err = ioutil.TempDir("", "")
		Expect(err).ToNot(HaveOccurred())

		config := `[{
			"dbid": 1,
			"port": 5432,
			"host": "localhost"
		}]`

		testutils.WriteOldConfig(dir, config)
		testutils.WriteNewConfig(dir, config)

		agentPort, err = testutils.GetOpenPort()
		Expect(err).ToNot(HaveOccurred())

		port, err = testutils.GetOpenPort()
		Expect(err).ToNot(HaveOccurred())

		conf := &hubServices.HubConfig{
			CliToHubPort:   port,
			HubToAgentPort: agentPort,
			StateDir:       dir,
		}

		reader := configutils.NewReader()

		outChan = make(chan []byte, 10)
		errChan = make(chan error, 10)
		hubExecer = &testutils.FakeCommandExecer{}
		hubExecer.SetOutput(&testutils.FakeCommand{
			Out: outChan,
			Err: errChan,
		})

		hub = hubServices.NewHub(&cluster.Pair{}, &reader, grpc.DialContext, hubExecer.Exec, conf)
		go hub.Start()
	})

	AfterEach(func() {
		hub.Stop()

		os.RemoveAll(dir)

		Expect(checkPortIsAvailable(port)).To(BeTrue())
		Expect(checkPortIsAvailable(agentPort)).To(BeTrue())
	})

	It("updates status PENDING to COMPLETE if successful", func() {
		Expect(runStatusUpgrade()).To(ContainSubstring("PENDING - Adjust upgrade cluster ports"))

		upgradeReconfigurePortsSession := runCommand("upgrade", "reconfigure-ports")
		Eventually(upgradeReconfigurePortsSession).Should(Exit(0))

		Expect(hubExecer.Calls()[0]).To(ContainSubstring("sed"))

		Expect(runStatusUpgrade()).To(ContainSubstring("COMPLETE - Adjust upgrade cluster ports"))

	})

	It("updates status to FAILED if it fails to run", func() {
		Expect(runStatusUpgrade()).To(ContainSubstring("PENDING - Adjust upgrade cluster ports"))

		errChan <- errors.New("fake test error, reconfigure-ports failed")

		upgradeShareOidsSession := runCommand("upgrade", "reconfigure-ports")
		Eventually(upgradeShareOidsSession).Should(Exit(1))

		Eventually(runStatusUpgrade()).Should(ContainSubstring("FAILED - Adjust upgrade cluster ports"))
	})
})
