package sshClient_test

import (
	"golang.org/x/crypto/ssh"

	"io"

	"bytes"

	"errors"

	"path"
	"runtime"

	"io/ioutil"

	"gp_upgrade/sshClient"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var (
	param_network string
	param_addr    string
	param_config  *ssh.ClientConfig
)

var _ = Describe("SSHConnector", func() {
	var (
		subject       *sshClient.RealSSHConnector
		test_key_path string
	)
	BeforeEach(func() {
		_, this_file_path, _, _ := runtime.Caller(0)
		test_key_path = path.Join(path.Dir(this_file_path), "../integrations/sshd/fake_private_key.pem")
		subject = &sshClient.RealSSHConnector{
			SSHDialer:      FakeDialer{},
			SSHKeyParser:   FakeKeyParser{},
			PrivateKeyPath: test_key_path,
		}

	})

	Describe("#New", func() {
		It("populates the private key correctly", func() {
			const PRIVATE_KEY_FILE_PATH = "/tmp/testPrivateKeyFile.key"
			ioutil.WriteFile(PRIVATE_KEY_FILE_PATH, []byte("----TEST PRIVATE KEY ---"), 0600)
			sshConnector, err := sshClient.NewSSHConnector(PRIVATE_KEY_FILE_PATH)
			Expect(err).ToNot(HaveOccurred())
			Expect(sshConnector.(*sshClient.RealSSHConnector).PrivateKeyPath).To(Equal(PRIVATE_KEY_FILE_PATH))
		})
		It("returns an error when private key is missing", func() {
			_, err := sshClient.NewSSHConnector("pathThatDoesNotExist")
			Expect(err).To(HaveOccurred())
		})
	})

	Describe("#Connect", func() {
		Context("Happy path connection", func() {
			It("returns a session", func() {
				_, err := subject.Connect("localhost", 22, "gpadmin")

				Expect(err).ToNot(HaveOccurred())
			})
			It("calls dial with correct parameters", func() {
				_, err := subject.Connect("localhost", 22, "gpadmin")

				Expect(err).ToNot(HaveOccurred())
				Expect(param_network).To(Equal("tcp"))
				Expect(param_addr).To(Equal("localhost:22"))
				Expect(param_config.User).To(Equal("gpadmin"))
				// docker container has ssh client library that requires a callback
				Expect(param_config.HostKeyCallback).ToNot(Equal(nil))
				Expect(len(param_config.Auth)).To(Equal(1))
			})
		})

		Context("errors", func() {
			Context("private key file cannot be opened", func() {
				It("returns an error message", func() {
					subject.PrivateKeyPath = "invalid_private_key"
					_, err := subject.Connect("", 0, "")

					Expect(err).To(HaveOccurred())
					Expect(err.Error()).To(Equal("open invalid_private_key: no such file or directory"))
				})
			})

			Context("private key file cannot be parsed", func() {
				It("returns an error message", func() {
					subject.SSHKeyParser = ThrowingKeyParser{}

					_, err := subject.Connect("", 0, "")

					Expect(err).To(HaveOccurred())
					Expect(err.Error()).To(Equal("test parsing failure"))
				})
			})

			Context("dialing connection returns error", func() {
				It("returns an error message", func() {
					subject.SSHDialer = ThrowingDialer{}

					_, err := subject.Connect("", 0, "")

					Expect(err).To(HaveOccurred())
					Expect(err.Error()).To(Equal("test dialing failure"))
				})
			})
			Context("new session returns error", func() {
				It("returns an error message", func() {
					subject.SSHDialer = ThrowingBadClientDialer{}

					_, err := subject.Connect("", 0, "")

					Expect(err).To(HaveOccurred())
					Expect(err.Error()).To(Equal("test NewSession failure"))
				})
			})
		})
	})

	Describe("#ConnectAndExecute", func() {
		Context("happy: when command runs successfully", func() {
			It("it returns the output from a command", func() {

				result, err := subject.ConnectAndExecute("localhost", 22, "gpadmin", "foo")

				Expect(err).ToNot(HaveOccurred())
				Expect(result).To(Equal("fake session output"))
			})
		})
		Context("errors", func() {
			It("returns an error when connect fails", func() {
				subject.SSHDialer = ThrowingDialer{}
				_, err := subject.ConnectAndExecute("", -1, "", "")
				Expect(err).To(HaveOccurred())
			})
			It("returns an error when Output has erroneous output", func() {
				subject.SSHDialer = GoodDialerBadOutput{}
				_, err := subject.ConnectAndExecute("", 0, "", "")
				Expect(err).To(HaveOccurred())
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

type FakeSSHClient struct {
	buf bytes.Buffer
}

func (FakeSSHClient) NewSession() (sshClient.SSHSession, error) {
	fakeSession := FakeSession{}
	return &fakeSession, nil
}

type FakeSession struct{}

func (fakeSession FakeSession) Close() error {
	return nil
}

func (fakeSession FakeSession) Output(string) ([]byte, error) {
	return []byte("fake session output"), nil
}

type ErrorOutputSession struct{}

func (errorOutputSession ErrorOutputSession) Close() error {
	return nil
}

func (errorOutputSession ErrorOutputSession) Output(string) ([]byte, error) {
	return nil, errors.New("test Output failure")
}

type FakeDialer struct{}

func (FakeDialer) Dial(network, addr string, config *ssh.ClientConfig) (sshClient.SSHClient, error) {
	param_network = network
	param_addr = addr
	param_config = config
	return &FakeSSHClient{}, nil
}

type GoodDialerBadOutput struct{}

func (GoodDialerBadOutput) Dial(network, addr string, config *ssh.ClientConfig) (sshClient.SSHClient, error) {
	return &GoodClientBadSession{}, nil
}

type GoodClientBadSession struct{}

func (GoodClientBadSession) NewSession() (sshClient.SSHSession, error) {
	return &ErrorOutputSession{}, nil
}

type ThrowingKeyParser struct{}

func (ThrowingKeyParser) ParsePrivateKey(pemBytes []byte) (ssh.Signer, error) {
	return nil, errors.New("test parsing failure")
}

type ThrowingDialer struct{}

func (ThrowingDialer) Dial(network, addr string, config *ssh.ClientConfig) (sshClient.SSHClient, error) {
	return nil, errors.New("test dialing failure")
}

type ThrowingClient struct{}

func (ThrowingClient) NewSession() (sshClient.SSHSession, error) {
	return nil, errors.New("test NewSession failure")
}

type ThrowingBadClientDialer struct{}

func (ThrowingBadClientDialer) Dial(network, addr string, config *ssh.ClientConfig) (sshClient.SSHClient, error) {
	return new(ThrowingClient), nil
}
