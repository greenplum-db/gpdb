package integrations_test

import (
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gexec"

	"os/exec"
	"testing"

	"os"

	"gp_upgrade/test_utils"

	"path"

	"golang.org/x/crypto/ssh"
)

func TestCommands(t *testing.T) {
	RegisterFailHandler(Fail)
	RunSpecs(t, "Integration Tests Suite")
}

var (
	commandPath string
	gConfig     *ssh.ServerConfig
	sshd        *exec.Cmd
)

var _ = BeforeEach(func() {
	path := os.Getenv("GOPATH")
	sshd = exec.Command(path + "/bin/test/sshd")
	_, err := sshd.StdoutPipe()
	test_utils.Check("cannot get stdout", err)
	_, err = sshd.StderrPipe()
	test_utils.Check("cannot get stderr", err)

	err = sshd.Start()
	Expect(err).ToNot(HaveOccurred())
})

var _ = AfterEach(func() {
	ShutDownSshdServer()
})

var _ = BeforeSuite(func() {
	var err error
	commandPath, err = Build("gp_upgrade") // if you want build flags, do a separate Build() in a specific integration test
	Expect(err).NotTo(HaveOccurred())
})

var _ = SynchronizedAfterSuite(func() {}, func() {
	CleanupBuildArtifacts()
})

func setPrivateKeyPermissions() {
	integrations_path := path.Join(os.Getenv("GOPATH"), "src/gp_upgrade/integrations")
	priv_keys := []string{
		"fixtures/registered_private_key.pem",
		"fixtures/unregistered_private_key.pem",
		"sshd/private_key.pem",
	}
	for _, key_path := range priv_keys {
		os.Chmod(path.Join(integrations_path, key_path), 0400)
	}
}

func runCommand(args ...string) *Session {

	// IMPORTANT TEST INFO: exec.Command forks and runs in a separate process,
	// which has its own Golang context; any mocks/fakes you set up in
	// the test context will NOT be meaningful in the new exec.Command context.
	cmd := exec.Command(commandPath, args...)
	session, err := Start(cmd, GinkgoWriter, GinkgoWriter)
	Expect(err).NotTo(HaveOccurred())
	<-session.Exited

	return session
}

type KeyPair struct {
	Key string
	Val string
}

func ShutDownSshdServer() {
	if sshd != nil {
		sshd.Process.Kill()
		sshd = nil
	}
}
