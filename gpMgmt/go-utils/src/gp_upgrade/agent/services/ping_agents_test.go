package services_test

import (
	"gp_upgrade/agent/services"
	pb "gp_upgrade/idl"
	"gp_upgrade/testutils"
	"gp_upgrade/utils"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("CommandListener", func() {
	BeforeEach(func() {
		testhelper.SetupTestLogger()
	})

	AfterEach(func() {
		//any mocking of utils.System function pointers should be reset by calling InitializeSystemFunctions
		utils.System = utils.InitializeSystemFunctions()
	})

	It("returns an empty reply", func() {
		commandExecer := &testutils.FakeCommandExecer{}
		commandExecer.SetOutput(&testutils.FakeCommand{})

		agent := services.NewAgentServer(commandExecer.Exec, services.AgentConfig{})

		_, err := agent.PingAgents(nil, &pb.PingAgentsRequest{})
		Expect(err).To(BeNil())
	})
})
