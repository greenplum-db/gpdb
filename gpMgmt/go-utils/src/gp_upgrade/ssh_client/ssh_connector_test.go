package ssh_client_test

import (
	"golang.org/x/crypto/ssh"

	"io"

	"bytes"

	"bufio"

	"os"

	"errors"

	"gp_upgrade/ssh_client"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var (
	param_network string
	param_addr    string
	param_config  *ssh.ClientConfig
)

var _ = Describe("SshConnector", func() {
	var (
		subject       ssh_client.SshConnector
		test_key_path string
	)
	BeforeEach(func() {
		subject = ssh_client.SshConnector{
			SshDialer:    FakeDialer{},
			SshKeyParser: FakeKeyParser{},
		}
		home := os.Getenv("HOME")
		test_key_path = home + "/workspace/gpdb/gpMgmt/go-utils/src/gp_upgrade/commands/sshd/private_key.pem"
	})

	Describe("#Connect", func() {
		Context("Happy path connection", func() {
			It("returns a session", func() {
				result, err := subject.Connect("localhost", 22, "gpadmin", test_key_path)

				Expect(err).ToNot(HaveOccurred())
				var buf []byte = make([]byte, 200)
				numread, _ := result.Stdin.Read(buf)
				Expect(string(buf[:numread])).To(Equal("test session"))
			})
			It("calls dial with correct parameters", func() {
				_, err := subject.Connect("localhost", 22, "gpadmin", test_key_path)

				Expect(err).ToNot(HaveOccurred())
				Expect(param_network).To(Equal("tcp"))
				Expect(param_addr).To(Equal("localhost:22"))
				Expect(param_config.User).To(Equal("gpadmin"))
			})
		})

		Context("errors", func() {
			Context("private key file cannot be opened", func() {
				It("returns an error message", func() {
					_, err := subject.Connect("", 0, "", "invalid_private_key")

					Expect(err).To(HaveOccurred())
					Expect(err.Error()).To(Equal("open invalid_private_key: no such file or directory"))
				})
			})

			Context("private key file cannot be parsed", func() {
				It("returns an error message", func() {
					subject.SshKeyParser = ThrowingKeyParser{}

					_, err := subject.Connect("", 0, "", test_key_path)

					Expect(err).To(HaveOccurred())
					Expect(err.Error()).To(Equal("test parsing failure"))
				})
			})

			Context("dialing connection returns error", func() {
				It("returns an error message", func() {
					subject.SshDialer = ThrowingDialer{}

					_, err := subject.Connect("", 0, "", test_key_path)

					Expect(err).To(HaveOccurred())
					Expect(err.Error()).To(Equal("test dialing failure"))
				})
			})
			Context("new session returns error", func() {
				It("returns an error message", func() {
					subject.SshDialer = ThrowingBadClientDialer{}

					_, err := subject.Connect("", 0, "", test_key_path)

					Expect(err).To(HaveOccurred())
					Expect(err.Error()).To(Equal("test newsession failure"))
				})
			})
		})
	})

})

type FakeSigner struct{}

func (signer FakeSigner) PublicKey() ssh.PublicKey {
	return nil
}
func (signer FakeSigner) Sign(rand io.Reader, data []byte) (*ssh.Signature, error) {
	return nil, nil
}

type FakeKeyParser struct{}

func (parser FakeKeyParser) ParsePrivateKey(pemBytes []byte) (ssh.Signer, error) {
	return FakeSigner{}, nil
}

type FakeSshClient struct {
	buf bytes.Buffer
}

func (fakeSshClient FakeSshClient) NewSession() (*ssh.Session, error) {
	fakeSshClient.buf.Write([]byte("test session"))
	reader := bufio.NewReader(&fakeSshClient.buf)
	result := &ssh.Session{Stdin: reader}
	return result, nil
}

type FakeDialer struct{}

func (dialer FakeDialer) Dial(network, addr string, config *ssh.ClientConfig) (ssh_client.SshClient, error) {
	param_network = network
	param_addr = addr
	param_config = config
	return &FakeSshClient{}, nil
}

type ThrowingKeyParser struct{}

func (parser ThrowingKeyParser) ParsePrivateKey(pemBytes []byte) (ssh.Signer, error) {
	return nil, errors.New("test parsing failure")
}

type ThrowingDialer struct{}

func (dialer ThrowingDialer) Dial(network, addr string, config *ssh.ClientConfig) (ssh_client.SshClient, error) {
	return nil, errors.New("test dialing failure")
}

type ThrowingBadClientDialer struct{}

type ThrowingClient struct{}

func (fakeSshClient ThrowingClient) NewSession() (*ssh.Session, error) {
	return nil, errors.New("test newsession failure")
}

func (badClientDialer ThrowingBadClientDialer) Dial(network, addr string, config *ssh.ClientConfig) (ssh_client.SshClient, error) {
	return new(ThrowingClient), nil
}
