package ssh_test

import gpssh "gp_upgrade/ssh"

import (
	"golang.org/x/crypto/ssh"

	"io"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("SshConnector", func() {
	var (
		subject gpssh.SshConnector
	)
	BeforeEach(func() {
		subject = gpssh.SshConnector{}
	})

	Describe("#Connect", func() {
		Context("Happy path", func() {
			It("returns a session", func() {
				subject.SshKeyParser = FakeKeyParser{}
				subject.SshDialer = FakeDialer{}

				_, err := subject.Connect("localhost", 22, "gpadmin", "/Users/pivotal/workspace/gpdb/gpMgmt/go-utils/src/gp_upgrade/commands/sshd/private_key.pem")

				Expect(err).ToNot(HaveOccurred())
			})
		})

		Context("private key file cannot be opened", func() {
			It("prints an error message and exits", func() {
				_, err := subject.Connect("", 0, "", "invalid_private_key")

				Expect(err).To(HaveOccurred())
				Expect(err.Error()).To(Equal("open invalid_private_key: no such file or directory"))
			})
		})
	})

})

type FakeSigner struct{}

func (signer FakeSigner) PublicKey() ssh.PublicKey { return nil }
func (signer FakeSigner) Sign(rand io.Reader, data []byte) (*ssh.Signature, error) {
	return nil, nil
}

type FakeKeyParser struct{}

func (parser FakeKeyParser) ParsePrivateKey(pemBytes []byte) (ssh.Signer, error) {
	return FakeSigner{}, nil
}

type FakeSshClient struct{}

func (fakeSshClient FakeSshClient) NewSession() (*ssh.Session, error) {
	return &ssh.Session{}, nil
}

type FakeDialer struct{}

func (dialer FakeDialer) Dial(network, addr string, config *ssh.ClientConfig) (gpssh.SshClient, error) {
	return &FakeSshClient{}, nil
}
