package integrations_test

import (
	"gp_upgrade/testUtils"
	"io/ioutil"

	"gp_upgrade/hub/configutils"

	"fmt"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"github.com/onsi/gomega/gbytes"
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

	Describe("seginstall", func() {
		It("updates status PENDING to COMPLETE if successful", func() {
			statusSessionPending := runCommand("status", "upgrade")
			Eventually(statusSessionPending).Should(gbytes.Say("PENDING - Install binaries on segments"))

			session := runCommand("check", "seginstall")
			Eventually(session).Should(Exit(0))

			//statusSession := runCommand("status", "upgrade")
			//Eventually(statusSession).Should(gbytes.Say("COMPLETE - Install binaries on segments"))
		})
	})
})
