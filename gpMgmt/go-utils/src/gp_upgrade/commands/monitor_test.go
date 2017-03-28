package commands_test

import (
	. "gp_upgrade/utils"

	"bytes"
	"encoding/binary"
	"fmt"

	"os"

	"io/ioutil"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gbytes"
	. "github.com/onsi/gomega/gexec"
)

const (
	grep_pg_upgrade = `
gpadmin            7520   0.0  0.0  2432772    676 s004  S+    3:56PM   0:00.00 grep pg_upgrade
pg_upgrade --verbose  --old-bindir /usr/local/greenplum-db-4.3.9.1/bin --new-bindir  /usr/local/greenplum-db-5/bin --old-datadir /data/gpdata/master/gpseg-1 --new-datadir /data/gp5data/master/gpseg-1 --old-port 5432 --new-port 6543 --link
`
	temp_home_dir = "/tmp/gp_upgrade_test_temp_home_dir"
)

var _ = Describe("monitor", func() {
	// todo replace CheatSheet, which uses file system as information transfer, to instead be a socket call on our running SSHD daemon to set up the next response
	AfterEach(func() {
		// remove any leftover cheatsheet (sshd fake reply)
		cheatSheet := CheatSheet{}
		cheatSheet.RemoveFile()

		err := os.RemoveAll(temp_home_dir)
		Check("cannot remote temp home dir", err)
	})

	Describe("when pg_upgrade is running on the target host", func() {
		It("reports that pg_upgrade is running", func() {
			cheatSheet := CheatSheet{Response: grep_pg_upgrade, ReturnCode: intToBytes(0)}
			cheatSheet.WriteToFile()

			session := runCommand("monitor", "--host", "localhost", "--segment_id", "42", "--port", "2022", "--private_key", "sshd/private_key.pem", "--user", "pivotal")

			Eventually(session).Should(Exit(0))
			Eventually(session.Out).Should(Say("pg_upgrade is running on host localhost"))
		})
	})

	Describe("when pg_upgrade process is NOT running", func() {
		It("reports that pg_upgrade is not running", func() {
			only_grep_itself := "gpadmin            7520   0.0  0.0  2432772    676 s004  S+    3:56PM   0:00.00 grep pg_upgrade"
			cheatSheet := CheatSheet{Response: only_grep_itself, ReturnCode: intToBytes(0)}
			cheatSheet.WriteToFile()

			session := runCommand("monitor", "--host", "localhost", "--segment_id", "42", "--port", "2022", "--private_key", "sshd/private_key.pem", "--user", "pivotal")

			Eventually(session).Should(Exit(0))
			expectedMsg := "pg_upgrade is not running on host localhost"
			Eventually(session.Out).Should(Say(expectedMsg))
		})
	})

	Describe("when SSH is not running at the remote end", func() {
		It("complains with standard ssh error phrasing", func() {
			ShutDownSshdServer()

			session := runCommand("monitor", "--host", "localhost", "--segment_id", "42", "--port", "2022", "--private_key", "sshd/private_key.pem", "--user", "pivotal")

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

	XDescribe("when the private key is found but does not succeed", func() {

	})

	Describe("when --private_key option is not provided", func() {
		Describe("when the default private key is found", func() {
			Describe("and the key works", func() {
				It("works", func() {
					save := SetHomeDir(temp_home_dir)
					cheatSheet := CheatSheet{Response: grep_pg_upgrade, ReturnCode: intToBytes(0)}
					cheatSheet.WriteToFile()
					path := os.Getenv("GOPATH")
					content, err := ioutil.ReadFile(path + "/src/gp_upgrade/commands/sshd/registered.priv")
					Check("cannot read private key file", err)
					err = os.MkdirAll(temp_home_dir+"/.ssh", 0700)
					Check("cannot create .ssh", err)
					ioutil.WriteFile(temp_home_dir+"/.ssh/id_rsa", content, 0500)
					Check("cannot write private key file", err)

					session := runCommand("monitor", "--host", "localhost", "--segment_id", "42", "--port", "2022", "--user", "pivotal")

					os.Setenv("HOME", save)
					Eventually(session).Should(Exit(0))
					Eventually(session.Out).Should(Say("pg_upgrade is running on host localhost"))
				})
			})
			Describe("and the key does not work", func() {
				It("complains", func() {
					save := SetHomeDir(temp_home_dir)
					cheatSheet := CheatSheet{Response: grep_pg_upgrade, ReturnCode: intToBytes(0)}
					cheatSheet.WriteToFile()
					path := os.Getenv("GOPATH")
					content, err := ioutil.ReadFile(path + "/src/gp_upgrade/commands/sshd/unregistered.priv")
					Check("cannot read private key file", err)
					err = os.MkdirAll(temp_home_dir+"/.ssh", 0700)
					Check("cannot create .ssh", err)
					ioutil.WriteFile(temp_home_dir+"/.ssh/id_rsa", content, 0500)
					Check("cannot write private key file", err)

					session := runCommand("monitor", "--host", "localhost", "--segment_id", "42", "--port", "2022", "--user", "pivotal")

					os.Setenv("HOME", save)
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

					session := runCommand("monitor", "--host", "localhost", "--segment_id", "42", "--port", "2022", "--user", "pivotal")

					os.Setenv("HOME", save)
					Eventually(session).Should(Exit(1))
					Eventually(session.Err).Should(Say("user has not specified a HOME environment value"))
				})
			})

			Describe("because there is no file at the default ssh location", func() {
				It("complains", func() {
					save := SetHomeDir("/tmp/gp_upgrade_test_temp_home_dir")

					session := runCommand("monitor", "--host", "localhost", "--segment_id", "42", "--port", "2022", "--user", "pivotal")

					os.Setenv("HOME", save)
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

			session := runCommand("monitor", "--host", "localhost", "--segment_id", "42", "--port", "2022", "--private_key", "sshd/private_key.pem", "--user", "pivotal")

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
