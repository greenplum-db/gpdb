package services_test

import (
	"gp_upgrade/utils"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"gp_upgrade/agent/services"
	"gp_upgrade/testutils"
	"io/ioutil"
	"os"
)

var _ = Describe("AgentServer", func() {
	var (
		dir       string
		agentConf services.AgentConfig
		exists    func() bool
	)

	BeforeEach(func() {
		testhelper.SetupTestLogger()
		dir, err := ioutil.TempDir("", "")
		Expect(err).ToNot(HaveOccurred())

		agentPort, err := testutils.GetOpenPort()
		Expect(err).ToNot(HaveOccurred())

		agentConf = services.AgentConfig{
			Port:     agentPort,
			StateDir: dir,
		}

		exists = func() bool {
			_, err := os.Stat(dir)
			if os.IsNotExist(err) {
				return false
			}
			return true
		}
	})

	AfterEach(func() {
		utils.System = utils.InitializeSystemFunctions()
	})

	It("starts if stateDir already exists", func() {
		agent := services.NewAgentServer(nil, agentConf)

		go agent.Start()
		defer agent.Stop()

		Eventually(exists).Should(BeTrue())
		os.RemoveAll(dir)
	})

	It("creates stateDir if none exists", func() {
		err := os.RemoveAll(dir)
		Expect(err).ToNot(HaveOccurred())
		_, err = os.Stat(dir)
		Expect(os.IsNotExist(err)).To(BeTrue())

		agent := services.NewAgentServer(nil, agentConf)
		go agent.Start()
		defer agent.Stop()

		Eventually(exists).Should(BeTrue())
		os.RemoveAll(dir)
	})
})
