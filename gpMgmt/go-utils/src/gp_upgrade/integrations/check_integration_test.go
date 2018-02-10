package integrations_test

import (
	"gp_upgrade/testUtils"
	"io/ioutil"

	"gp_upgrade/hub/configutils"

	"fmt"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gexec"
)

// needs the cli and the hub
var _ = Describe("check", func() {

	BeforeEach(func() {
		ensureHubIsUp()
	})

	Describe("when a greenplum master db on localhost is up and running", func() {
		It("happy: the database configuration is saved to a specified location", func() {
			session := runCommand("check", "config", "--master-host", "localhost")

			if session.ExitCode() != 0 {
				fmt.Println("make sure greenplum is running")
			}
			Eventually(session).Should(Exit(0))
			// check file

			_, err := ioutil.ReadFile(configutils.GetConfigFilePath())
			testUtils.Check("cannot read file", err)

			reader := configutils.Reader{}
			reader.OfOldClusterConfig()
			err = reader.Read()
			testUtils.Check("cannot read config", err)

			// for extra credit, read db and compare info
			Expect(len(reader.GetSegmentConfiguration())).To(BeNumerically(">", 1))

			// should there be something checking the version file being laid down as well?
		})
	})

	// `gp_backup check seginstall` verifies that the user has installed the software on all hosts
	// As a single-node check, this test verifies the mechanics of the check, but would typically succeed.
	// The implementation, however, uses the gp_upgrade_agent binary to verify installation. In real life,
	// all the binaries, gp_upgrade_hub and gp_upgrade_agent included, would be alongside each other.
	// But in our integration tests' context, only the necessary Golang code is compiled, and Ginkgo's default
	// is to compile gp_upgrade_hub and gp_upgrade_agent in separate directories. As such, this test depends on the
	// setup in `integrations_suite_test.go` to replicate the real-world scenario of "install binaries side-by-side".
	//
	// TODO: This test might be interesting to run multi-node; for that, figure out how "installation" should be done
	Describe("seginstall", func() {
		It("updates status PENDING to RUNNING then to COMPLETE if successful", func() {
			runCommand("check", "config")
			Expect(runStatusUpgrade()).To(ContainSubstring("PENDING - Install binaries on segments"))

			expectationsDuringCommandInFlight := make(chan bool)

			go func() {
				// TODO: Can this flake? if the in-progress window is shorter than the frequency of Eventually(), then yea
				Eventually(runStatusUpgrade).Should(ContainSubstring("RUNNING - Install binaries on segments"))
				expectationsDuringCommandInFlight <- true
			}()

			session := runCommand("check", "seginstall")
			Eventually(session).Should(Exit(0))
			<-expectationsDuringCommandInFlight

			Eventually(runStatusUpgrade).Should(ContainSubstring("COMPLETE - Install binaries on segments"))
		})
	})
})

func runStatusUpgrade() string {
	return string(runCommand("status", "upgrade").Out.Contents())
}
