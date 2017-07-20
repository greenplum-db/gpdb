package integrations_test

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"io/ioutil"
	"os"
	"path"
	"runtime"

	. "gp_upgrade/testUtils"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gbytes"
	. "github.com/onsi/gomega/gexec"
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
	)

	BeforeEach(func() {
		_, this_file_path, _, _ := runtime.Caller(0)
		private_key_path = path.Join(path.Dir(this_file_path), "sshd/fake_private_key.pem")
		fixture_path = path.Join(path.Dir(this_file_path), "fixtures")
		save_home_dir = ResetTempHomeDir()
		WriteSampleConfig()
	})
	AfterEach(func() {
		// todo replace CheatSheet, which uses file system as information transfer, to instead be a socket call on our running SSHD daemon to set up the next response
		// remove any leftover cheatsheet (sshd fake reply)
		cheatSheet := CheatSheet{}
		cheatSheet.RemoveFile()

		os.Setenv("HOME", save_home_dir)
	})

	Describe("when pg_upgrade is running on the target host", func() {
		It("happy: reports that pg_upgrade is running", func() {
			cheatSheet := CheatSheet{Response: GREP_PG_UPGRADE, ReturnCode: intToBytes(0)}
			cheatSheet.WriteToFile()

			session := runCommand("monitor", "--host", "localhost", "--segment-id", "7", "--port", "2022", "--private_key", private_key_path, "--user", "pivotal")

			Eventually(session).Should(Exit(0))
			Eventually(session.Out).Should(Say(fmt.Sprintf(`pg_upgrade state - active {"segment_id":%d,"host":"%s"}`, 7, "localhost")))
		})
	})

	Describe("when host and segment ID are not provided", func() {
		It("complains", func() {
			session := runCommand("monitor")

			Eventually(session).Should(Exit(1))
			Eventually(session.Err).Should(Say("the required flags `--host' and `--segment-id' were not specified"))
		})
	})

	Describe("when --private_key option is not provided", func() {
		Describe("when the default private key is found", func() {
			Describe("and the key works", func() {
				It("works", func() {
					cheatSheet := CheatSheet{Response: GREP_PG_UPGRADE, ReturnCode: intToBytes(0)}
					cheatSheet.WriteToFile()
					content, err := ioutil.ReadFile(path.Join(fixture_path, "registered_private_key.pem"))
					Check("cannot read private key file", err)
					err = os.MkdirAll(TempHomeDir+"/.ssh", 0700)
					Check("cannot create .ssh", err)
					ioutil.WriteFile(TempHomeDir+"/.ssh/id_rsa", content, 0500)
					Check("cannot write private key file", err)

					session := runCommand("monitor", "--host", "localhost", "--segment-id", "7", "--port", "2022", "--user", "pivotal")

					Eventually(session).Should(Exit(0))
					Eventually(session.Out).Should(Say(fmt.Sprintf(`pg_upgrade state - active {"segment_id":%d,"host":"%s"}`, 7, "localhost")))
				})
			})
		})
	})
})

func intToBytes(i uint32) []byte {
	buf := new(bytes.Buffer)
	err := binary.Write(buf, binary.BigEndian, i)
	if err != nil {
		panic(fmt.Sprintf("binary.Write failed: %v", err))
	}
	return buf.Bytes()
}
