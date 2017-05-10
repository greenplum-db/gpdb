package backup_test

import (
	//"backup_restore/utils"
	//"fmt"
	"os/exec"
	"testing"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	//"github.com/onsi/gomega/gbytes"
	"github.com/onsi/gomega/gexec"
)

var gpbackupPath = ""

// Helper function to execute gpbackup and return a session for stdout checking
func gpbackup() *gexec.Session {
	command := exec.Command(gpbackupPath, "-dbname", "testdb")
	session, err := gexec.Start(command, GinkgoWriter, GinkgoWriter)
	Expect(err).ShouldNot(HaveOccurred())
	<-session.Exited
	return session
}

func TestBackup(t *testing.T) {
	RegisterFailHandler(Fail)
	RunSpecs(t, "backup tests")
}

var _ = Describe("backup integration tests", func() {
	/*	BeforeSuite(func() {
			var err error
			gpbackupPath, err = gexec.Build("backup_restore")
			Expect(err).ShouldNot(HaveOccurred())
			exec.Command("dropdb", "testdb").Run()
			err = exec.Command("createdb", "testdb").Run()
			if err != nil {
				Fail(fmt.Sprintf("%v", err))
			}
		})
		AfterSuite(func() {
			gexec.CleanupBuildArtifacts()
			exec.Command("dropdb", "testdb").Run()
		})

		Describe("transactionality tests", func() {
			Context("gpbackup runs its queries in a transaction", func () {
				It("Does not see records added during the transaction", func() {
					interference := utils.NewDBConn("testdb")
					interference.Connect()
					interference.Exec("CREATE TABLE foo(i int)")
					interference.Exec("INSERT INTO foo SELECT generate_series(1,100)")
					go func() {
						interference.Exec("SELECT pg_sleep(1); INSERT INTO foo SELECT generate_series(101,200)")
					}()
					session := gpbackup()
					Eventually(session.Out).Should(gbytes.Say("100"))
					Eventually(session.Out).ShouldNot(gbytes.Say("200"))
				})
			})
		})*/
})
