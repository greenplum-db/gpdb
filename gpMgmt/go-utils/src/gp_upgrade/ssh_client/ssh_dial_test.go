package ssh_client_test

import (
	"gp_upgrade/ssh_client"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"golang.org/x/crypto/ssh"
)

var _ = Describe("SshConnector", func() {

	Describe("#Dial", func() {
		It("returns an error when dialing fails", func() {
			subject := &ssh_client.RealDialer{}
			proxy, err := subject.Dial("thereisnohostnamedthis", "thereisnoaddresslikethis", &ssh.ClientConfig{})
			Expect(err).To(HaveOccurred())
			Expect(proxy).To(BeAssignableToTypeOf(ssh_client.RealClientProxy{}))
		})
	})

})
