package commands

import (
	"os"
	"path"
	"runtime"

	. "gp_upgrade/test_utils"

	"gp_upgrade/config"
	"io/ioutil"

	"gp_upgrade/ssh_client"

	"fmt"

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
		save_home_dir    string
		private_key_path string
		fixture_path     string
		subject          MonitorCommand
		buffer           *gbytes.Buffer
	)

	BeforeEach(func() {
		_, this_file_path, _, _ := runtime.Caller(0)
		private_key_path = path.Join(path.Dir(this_file_path), "sshd/private_key.pem")
		fixture_path = path.Join(path.Dir(this_file_path), "fixtures")
		save_home_dir = ResetTempHomeDir()
		WriteSampleConfig()

		subject = MonitorCommand{SegmentId: 7}

		buffer = gbytes.NewBuffer()
	})
	AfterEach(func() {
		os.Setenv("HOME", save_home_dir)
	})

	Describe("when pg_upgrade is running on the target host", func() {
		XIt("happy: reports that pg_upgrade is running", func() {
			fake := &FailingSshConnecter{}

			// todo we need to be able to mock out the ssh client so that a session can return a fake remote shell result
			err := subject.execute(fake, buffer)

			Expect(err).ToNot(HaveOccurred())

		})
		It("happy: it uses the default user for ssh connection when the user doesn't supply a ssh user", func() {
			subject.User = ""
			fake := &FailingSshConnecter{}

			subject.execute(fake, buffer)

			Expect(fake.user).ToNot(Equal(""))
		})
		It("parses ps output correctly", func() {
			fake := &SucceedingSshConnector{}

			err := subject.execute(fake, buffer)

			Expect(err).ToNot(HaveOccurred())
			Expect(string(buffer.Contents())).To(ContainSubstring(fmt.Sprintf(`pg_upgrade state - active`)))
		})

	})

	Describe("errors", func() {
		It("returns an error when the configuration cannot be read", func() {
			fake := &FailingSshConnecter{}
			os.RemoveAll(config.GetConfigFilePath())

			err := subject.execute(fake, buffer)

			Expect(err).To(HaveOccurred())
		})
		It("returns an error when the configuration has no entry for the segment-id specified by user", func() {
			fake := &FailingSshConnecter{}
			ioutil.WriteFile(config.GetConfigFilePath(), []byte("[]"), 0600)
			err := subject.execute(fake, buffer)

			Expect(err).To(HaveOccurred())
			Expect(err.Error()).To(ContainSubstring("not known in this cluster configuration"))
		})
		Context("when ssh connector fails", func() {
			It("returns an error", func() {
				fake := &FailingSshConnecter{}

				err := subject.execute(fake, buffer)

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

func (fakesshconnector FailingSshConnecter) Connect(Host string, Port int, user string) (ssh_client.Session, error) {
	return nil, errors.New("fake connect error")
}
func (fakesshconnector *FailingSshConnecter) ConnectAndExecute(Host string, Port int, user string, command string) (string, error) {
	fakesshconnector.user = user
	return "", errors.New("fake ConnectAndExecute error")
}

type SucceedingSshConnector struct{}

func (fakesshconnector SucceedingSshConnector) Connect(Host string, Port int, user string) (ssh_client.Session, error) {
	return nil, nil
}
func (fakesshconnector *SucceedingSshConnector) ConnectAndExecute(Host string, Port int, user string, command string) (string, error) {
	return GREP_PG_UPGRADE, nil
}
