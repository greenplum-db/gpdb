package commands_test

import (
	"os"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gexec"
)

var _ = Describe("check", func() {
	Describe("the database is running, master_host is provided, and connection is successful", func() {
		It("writes a file to ~/.gp_upgrade/cluster_config.json", func() {

			session := runCommand("check", "--master_host", "localhost")

			Eventually(session).Should(Exit(0))

			_, err := os.Open(os.Getenv("HOME") + "/.gp_upgrade/cluster_config.json")
			Expect(err).NotTo(HaveOccurred())

			// todo check the contents!
		})
	})
})
