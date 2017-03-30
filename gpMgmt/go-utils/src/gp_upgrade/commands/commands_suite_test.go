package commands_test

import (
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gexec"

	"os/exec"
	"testing"

	"os"

	"fmt"

	"strings"

	"gp_upgrade/utils"

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
)

var _ = BeforeEach(func() {
	path := os.Getenv("GOPATH")
	sshd = exec.Command(path + "/bin/test/sshd")
	_, err := sshd.StdoutPipe()
	utils.Check("cannot get stdout", err)
	_, err = sshd.StderrPipe()
	utils.Check("cannot get stderr", err)

	err = sshd.Start()
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
	setPrivateKeyPermissions()
})

var _ = SynchronizedAfterSuite(func() {}, func() {
	CleanupBuildArtifacts()
})

func setPrivateKeyPermissions() {
	command_path := os.Getenv("GOPATH") + "/src/gp_upgrade/commands/"
	priv_keys := []string{
		"fixtures/registered_private_key.pem",
		"fixtures/unregistered_private_key.pem",
		"sshd/private_key.pem",
	}
	for _, path := range priv_keys {
		os.Chmod(command_path+path, 0400)
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

// env is a list of key=value pairs, one pair per string entry in list
func runCommandWithEnv(additionalKeypairs []KeyPair, args ...string) *Session {

	localEnv := os.Environ()
	for _, keypairStr := range localEnv {
		keyVal := strings.Split(keypairStr, "=")
		// todo need map of additionalKeys
		if keyVal[0] == "" {
			// todo
		}
	}
	for _, keypair := range additionalKeypairs {
		localEnv = append(localEnv, fmt.Sprintf("%s=%s", keypair.Key, keypair.Val))
	}

	// IMPORTANT TEST INFO: exec.Command forks and runs in a separate process,
	// which has its own Golang context; any mocks/fakes you set up in
	// the test context will NOT be meaningful in the new exec.Command context.
	cmd := exec.Command(commandPath, args...)
	cmd.Env = localEnv
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
