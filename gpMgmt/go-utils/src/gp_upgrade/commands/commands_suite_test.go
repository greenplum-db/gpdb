package commands_test

import (
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gexec"

	"io"
	"os/exec"
	"testing"

	"os"

	"golang.org/x/crypto/ssh"
)

func TestCommands(t *testing.T) {
	RegisterFailHandler(Fail)
	RunSpecs(t, "Commands Suite")
}

var (
	commandPath string
	gConfig     *ssh.ServerConfig
	sshd        *exec.Cmd
	stdout      io.ReadCloser
	stderr      io.ReadCloser
)

var _ = BeforeEach(func() {
	path := os.Getenv("GOPATH")
	sshd = exec.Command(path + "/bin/test/sshd")
	stdout, _ = sshd.StdoutPipe()
	stderr, _ = sshd.StderrPipe()

	err := sshd.Start()
	Expect(err).ToNot(HaveOccurred())
})

var _ = AfterEach(func() {
	ShutDownSshdServer()
})

var _ = SynchronizedBeforeSuite(func() []byte {
	executable_path, err := Build("gp_upgrade")
	Expect(err).NotTo(HaveOccurred())
	return []byte(executable_path)
}, func(data []byte) {
	commandPath = string(data)
})

var _ = SynchronizedAfterSuite(func() {}, func() {
	CleanupBuildArtifacts()
})

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

func ShutDownSshdServer() {
	if sshd != nil {
		sshd.Process.Kill()
		sshd = nil
	}
}
