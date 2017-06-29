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

	"fmt"
	"strings"
)

func TestCommands(t *testing.T) {
	RegisterFailHandler(Fail)
	RunSpecs(t, "Integration Tests Suite")
}

var (
	commandPath string
	sshd        *exec.Cmd
)

var _ = BeforeEach(func() {
	gopaths := strings.Split(os.Getenv("GOPATH"), ":")
	binary_path := ""
	for i := 0; i < len(gopaths); i++ {
		one_path := path.Join(gopaths[i], "/bin/test/sshd")
		if _, err := os.Stat(one_path); !os.IsNotExist(err) {
			if len(binary_path) > 0 {
				Fail(fmt.Sprintf("More than one path to sshd binary \n%v\n", gopaths))
			} else {
				binary_path = one_path
			}
		}
	}
	sshd = exec.Command(binary_path)
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
