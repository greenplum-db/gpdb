package sshClient_test

import (
	"gp_upgrade/sshClient"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"golang.org/x/crypto/ssh"
)

var _ = Describe("SSHConnector", func() {

	Describe("#Dial", func() {
		It("returns an error when dialing fails", func() {
			subject := &sshClient.RealDialer{}
			proxy, err := subject.Dial("thereisnohostnamedthis", "thereisnoaddresslikethis", &ssh.ClientConfig{})
			Expect(err).To(HaveOccurred())
			Expect(proxy).To(BeAssignableToTypeOf(sshClient.RealClientProxy{}))
		})
	})

})
