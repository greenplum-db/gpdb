package integrations_test

import (
	"gp_upgrade/testUtils"
	"io/ioutil"

	"gp_upgrade/hub/configutils"

	"fmt"

	"time"

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

	Describe("seginstall", func() {
		//can we assert on the order of the states, not just their existence?
		It("updates status PENDING to RUNNING then to COMPLETE if successful", func() {
			runCommand("check", "config")
			Expect(runStatusUpgrade()).To(ContainSubstring("PENDING - Install binaries on segments"))

			session := runCommand("check", "seginstall")
			Eventually(session).Should(Exit(0))

			// in progress RUNNING state is covered in unit tests
			//time.Sleep(1 * time.Second)
			//statusSession := runCommand("status", "upgrade")
			//Eventually(string(statusSession.Out.Contents()), 3*).To(ContainSubstring("COMPLETE - Install binaries on segments"))

			Eventually(runStatusUpgrade, 2*time.Second).Should(ContainSubstring("COMPLETE - Install binaries on segments"))
		})
	})
})

func runStatusUpgrade() string {
	return string(runCommand("status", "upgrade").Out.Contents())
}
