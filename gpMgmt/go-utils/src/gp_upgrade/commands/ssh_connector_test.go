package commands_test

import (
	"gp_upgrade/commands"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("SshConnector", func() {
	var (
		subject commands.SshConnector
	)
	BeforeEach(func() {
		subject = commands.SshConnector{}
	})

	Describe("#Connect", func() {
		Context("Happy path", func() {
			It("returns a session", func() {
				//guarantor := commands.NewPrivateKeyGuarantor()
				//value := guarantor.Check("foo")
				//Expect(value).To(Equal("foo"))
			})
		})

		Context("private key file cannot be opened", func() {
			It("prints an error message and exits", func() {
				_, err := subject.Connect("", 0, "", "invalid_private_key")

				Expect(err).To(HaveOccurred())
				// todo expect message
			})
		})
	})

})
