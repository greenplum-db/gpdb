package integrations_test

import (
	"gp_upgrade/test_utils"
	"io/ioutil"
	"os"

	"gp_upgrade/config"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gexec"
)

var _ = Describe("check", func() {

	var (
		save_home_dir string
	)

	BeforeEach(func() {
		save_home_dir = test_utils.ResetTempHomeDir()
	})
	AfterEach(func() {
		os.Setenv("HOME", save_home_dir)
	})

	Describe("when a greenplum master db on localhost is up and running", func() {
		It("happy: the database configuration is saved to a specified location", func() {
			session := runCommand("check", "--master-host", "localhost")

			Eventually(session).Should(Exit(0))
			// check file

			_, err := ioutil.ReadFile(config.GetConfigFilePath())
			test_utils.Check("cannot read file", err)

			reader := config.Reader{}
			err = reader.Read()
			test_utils.Check("cannot read config", err)

			// for extra credit, read db and compare info
			Expect(len(reader.GetSegmentConfiguration())).To(BeNumerically(">", 1))
		})
	})
})
