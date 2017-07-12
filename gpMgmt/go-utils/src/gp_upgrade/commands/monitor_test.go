package commands

import (
	"os"

	"gp_upgrade/config"
	"gp_upgrade/shell_parsers"
	"io/ioutil"

	"gp_upgrade/ssh_client"

	"fmt"

	"gp_upgrade/test_utils"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"github.com/onsi/gomega/gbytes"
	"github.com/pkg/errors"
)

const (
	GREP_PG_UPGRADE = `
gpadmin            7520   0.0  0.0  2432772    676 s004  S+    3:56PM   0:00.00 grep pg_upgrade
pg_upgrade --verbose  --old-bindir /usr/local/greenplum-db-4.3.9.1/bin --new-bindir  /usr/local/greenplum-db-5/bin --old-datadir /data/gpdata/master/gpseg-1 --new-datadir /data/gp5data/master/gpseg-1 --old-port 25437 --new-port 6543 --link
`
)

var _ = Describe("monitor", func() {

	var (
		save_home_dir string
		subject       MonitorCommand
		buffer        *gbytes.Buffer
		shellParser   *shell_parsers.RealShellParser
	)

	BeforeEach(func() {
		save_home_dir = test_utils.ResetTempHomeDir()
		test_utils.WriteSampleConfig()

		subject = MonitorCommand{SegmentId: 7}

		shellParser = &shell_parsers.RealShellParser{}

		buffer = gbytes.NewBuffer()
	})

	AfterEach(func() {
		os.Setenv("HOME", save_home_dir)
	})

	Describe("when pg_upgrade status can be determined on remote host", func() {
		It("happy: it uses the default user for ssh connection when the user doesn't supply a ssh user", func() {
			subject.User = ""
			fake := &FailingSshConnecter{}

			subject.execute(fake, shellParser, buffer)

			Expect(fake.user).ToNot(Equal(""))
		})

		It("parses 'active' status correctly", func() {
			fake := &SucceedingSshConnector{}

			err := subject.execute(fake, shellParser, buffer)

			Expect(err).ToNot(HaveOccurred())
			Expect(string(buffer.Contents())).To(ContainSubstring(fmt.Sprintf(`pg_upgrade state - active`)))
			Expect(string(buffer.Contents())).To(HaveSuffix("\n"))
		})

		It("parses 'inactive' status correctly", func() {
			fake := &SucceedingSshConnector{}
			inactiveParser := &InactiveShellParser{}

			err := subject.execute(fake, inactiveParser, buffer)
			Expect(err).ToNot(HaveOccurred())
			Expect(string(buffer.Contents())).To(ContainSubstring("inactive"))
			Expect(string(buffer.Contents())).To(HaveSuffix("\n"))
		})
	})

	Describe("errors", func() {
		It("returns an error when the configuration cannot be read", func() {
			fake := &FailingSshConnecter{}
			os.RemoveAll(config.GetConfigFilePath())

			err := subject.execute(fake, shellParser, buffer)

			Expect(err).To(HaveOccurred())
		})

		It("returns an error when the configuration has no entry for the segment-id specified by user", func() {
			fake := &FailingSshConnecter{}
			ioutil.WriteFile(config.GetConfigFilePath(), []byte("[]"), 0600)
			err := subject.execute(fake, shellParser, buffer)

			Expect(err).To(HaveOccurred())
			Expect(err.Error()).To(ContainSubstring("not known in this cluster configuration"))
		})

		Context("when ssh connector fails", func() {
			It("returns an error", func() {
				fake := &FailingSshConnecter{}

				err := subject.execute(fake, shellParser, buffer)

				Expect(err).To(HaveOccurred())
			})
		})
	})

	Describe("errors", func() {
		Context("when private key is not found", func() {
			It("returns an error", func() {
				subject.PrivateKey = "thisisaninvalidpath"

				err := subject.Execute(nil)

				Expect(err).To(HaveOccurred())
			})
		})
	})
})

type FailingSshConnecter struct {
	user string
}

func (sshConnector FailingSshConnecter) Connect(Host string, Port int, user string) (ssh_client.SshSession, error) {
	return nil, errors.New("fake connect error")
}
func (sshConnector *FailingSshConnecter) ConnectAndExecute(Host string, Port int, user string, command string) (string, error) {
	sshConnector.user = user
	return "", errors.New("fake ConnectAndExecute error")
}

type SucceedingSshConnector struct{}

func (sshConnector SucceedingSshConnector) Connect(Host string, Port int, user string) (ssh_client.SshSession, error) {
	return nil, nil
}
func (sshConnector SucceedingSshConnector) ConnectAndExecute(Host string, Port int, user string, command string) (string, error) {
	return GREP_PG_UPGRADE, nil
}

type InactiveShellParser struct{}

func (InactiveShellParser) SetOutput(_ string) {}
func (InactiveShellParser) IsPgUpgradeRunning(_ int, _ string) bool {
	return false
}
