package commands_test

import (
	"bytes"
	"encoding/binary"
	"fmt"
	"os"
	"io/ioutil"
	"path"
	"runtime"

	. "gp_upgrade/test_utils"

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
		this_file_dir    string
	)

	BeforeEach(func() {
		_, this_file_dir, _, _ := runtime.Caller(0)
		private_key_path = path.Dir(this_file_dir) + "/sshd/private_key.pem"
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
		It("reports that pg_upgrade is running", func() {
			cheatSheet := CheatSheet{Response: GREP_PG_UPGRADE, ReturnCode: intToBytes(0)}
			cheatSheet.WriteToFile()

			session := runCommand("monitor", "--host", "localhost", "--segment_id", "7", "--port", "2022", "--private_key", private_key_path, "--user", "pivotal")

			Eventually(session).Should(Exit(0))
			Eventually(session.Out).Should(Say("pg_upgrade is running on host localhost"))
		})
	})

	Describe("when pg_upgrade process is NOT running", func() {
		It("reports that pg_upgrade is not running", func() {
			only_grep_itself := "gpadmin            7520   0.0  0.0  2432772    676 s004  S+    3:56PM   0:00.00 grep pg_upgrade"
			cheatSheet := CheatSheet{Response: only_grep_itself, ReturnCode: intToBytes(0)}
			cheatSheet.WriteToFile()

			session := runCommand("monitor", "--host", "localhost", "--segment_id", "7", "--port", "2022", "--private_key", private_key_path, "--user", "pivotal")

			Eventually(session).Should(Exit(0))
			expectedMsg := "pg_upgrade is not running on host localhost"
			Eventually(session.Out).Should(Say(expectedMsg))
		})
	})

	Describe("when segmentId provided is not in config", func() {
		It("reports unknown segmentId and returns 1", func() {
			only_grep_itself := "gpadmin            7520   0.0  0.0  2432772    676 s004  S+    3:56PM   0:00.00 grep pg_upgrade"
			unknown_segment_id := "8"
			cheatSheet := CheatSheet{Response: only_grep_itself, ReturnCode: intToBytes(0)}
			cheatSheet.WriteToFile()

			session := runCommand("monitor", "--host", "localhost", "--segment_id", unknown_segment_id, "--port", "2022", "--private_key", private_key_path, "--user", "pivotal")

			Eventually(session).Should(Exit(1))
			expectedMsg := fmt.Sprintf("segment_id %s not known in this cluster configuration", unknown_segment_id)
			Eventually(session.Err).Should(Say(expectedMsg))
		})
	})

	Describe("when SSH is not running at the remote end", func() {
		It("complains with standard ssh error phrasing", func() {
			ShutDownSshdServer()

			session := runCommand("monitor", "--host", "localhost", "--segment_id", "7", "--port", "2022", "--private_key", private_key_path, "--user", "pivotal")

			Eventually(session).Should(Exit(1))
			Eventually(session.Err).Should(Say("getsockopt: connection refused"))
		})
	})

	Describe("when host and segment ID are not provided", func() {
		It("complains", func() {
			session := runCommand("monitor")

			Eventually(session).Should(Exit(1))
			Eventually(session.Err).Should(Say("the required flags `--host' and `--segment_id' were not specified"))
		})
	})

	Describe("when the private key is found but ssh does not succeed", func() {
		It("complains", func() {
			invalid_private_key_path := path.Dir(this_file_dir) + "/sshd/invalid_private_key.pem"
			session := runCommand("monitor", "--host", "localhost", "--segment_id", "7", "--port", "2022", "--private_key", invalid_private_key_path, "--user", "pivotal")
			Eventually(session).Should(Exit(1))
			Eventually(session.Err).Should(Say("ssh: no key found"))
		})
	})

	Describe("when --private_key option is not provided", func() {
		Describe("when the default private key is found", func() {
			Describe("and the key works", func() {
				It("works", func() {
					cheatSheet := CheatSheet{Response: GREP_PG_UPGRADE, ReturnCode: intToBytes(0)}
					cheatSheet.WriteToFile()
					path := os.Getenv("GOPATH")
					content, err := ioutil.ReadFile(path + "/src/gp_upgrade/commands/fixtures/registered_private_key.pem")
					Check("cannot read private key file", err)
					err = os.MkdirAll(TempHomeDir+"/.ssh", 0700)
					Check("cannot create .ssh", err)
					ioutil.WriteFile(TempHomeDir+"/.ssh/id_rsa", content, 0500)
					Check("cannot write private key file", err)

					session := runCommand("monitor", "--host", "localhost", "--segment_id", "7", "--port", "2022", "--user", "pivotal")

					Eventually(session).Should(Exit(0))
					Eventually(session.Out).Should(Say("pg_upgrade is running on host localhost"))
				})
			})
			Describe("and the key does not work", func() {
				It("complains", func() {
					cheatSheet := CheatSheet{Response: GREP_PG_UPGRADE, ReturnCode: intToBytes(0)}
					cheatSheet.WriteToFile()
					path := os.Getenv("GOPATH")
					content, err := ioutil.ReadFile(path + "/src/gp_upgrade/commands/fixtures/unregistered_private_key.pem")
					Check("cannot read private key file", err)
					err = os.MkdirAll(TempHomeDir+"/.ssh", 0700)
					Check("cannot create .ssh", err)
					ioutil.WriteFile(TempHomeDir+"/.ssh/id_rsa", content, 0500)
					Check("cannot write private key file", err)

					session := runCommand("monitor", "--host", "localhost", "--segment_id", "7", "--port", "2022", "--user", "pivotal")

					Eventually(session).Should(Exit(1))
					Eventually(session.Err).Should(Say("ssh: handshake failed: ssh: unable to authenticate, attempted methods"))
					Eventually(session.Err).Should(Say(" no supported methods remain"))
				})
			})
		})

		Describe("when the default private key cannot be found", func() {
			Describe("because HOME is not set", func() {
				It("complains", func() {
					save := os.Getenv("HOME")
					os.Setenv("HOME", "")
					defer os.Setenv("HOME", save)

					session := runCommand("monitor", "--host", "localhost", "--segment_id", "7", "--port", "2022", "--user", "pivotal")

					Eventually(session).Should(Exit(1))
					Eventually(session.Err).Should(Say("user has not specified a HOME environment value"))
				})
			})

			Describe("because there is no file at the default ssh location", func() {
				It("complains", func() {
					session := runCommand("monitor", "--host", "localhost", "--segment_id", "7", "--port", "2022", "--user", "pivotal")

					Eventually(session).Should(Exit(1))
					Eventually(session.Err).Should(Say("open /tmp/gp_upgrade_test_temp_home_dir/.ssh/id_rsa: no such file or directory"))
				})
			})
		})
	})

	Describe("when the remote ssh command fails", func() {
		It("complains", func() {
			cheatSheet := CheatSheet{Response: "foo output", ReturnCode: intToBytes(1)}
			cheatSheet.WriteToFile()

			session := runCommand("monitor", "--host", "localhost", "--segment_id", "7", "--port", "2022", "--private_key", private_key_path, "--user", "pivotal")

			Eventually(session).Should(Exit(1))
			expectedMsg := "cannot run pgrep command on remote host, output: foo output\nError: Process exited with status 1"
			Eventually(session.Err).Should(Say(expectedMsg))
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
