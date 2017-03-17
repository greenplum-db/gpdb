package commands_test

import (
	. "gp_upgrade/common"

	"bytes"
	"encoding/binary"
	"fmt"

	"os"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gbytes"
	. "github.com/onsi/gomega/gexec"
)

var _ = Describe("monitor", func() {
	// todo replace CheatSheet, which uses file system as information transfer, to instead be a socket call on our running SSHD daemon to set up the next response
	AfterEach(func() {
		// remove any leftover cheatsheet (sshd fake reply)
		cheatSheet := CheatSheet{}
		cheatSheet.RemoveFile()
	})

	Describe("if SSH is not running at the remote end", func() {
		It("complains with standard ssh error phrasing", func() {
			ShutDownSshdServer()
			session := runCommand("monitor", "--host", "localhost", "--segment_id", "42", "--port", "2022", "--private_key", "sshd/private_key.pem", "--user", "pivotal")

			Eventually(session).Should(Exit(1))

			// typical error message from SSH
			expectedMsg := "getsockopt: connection refused"
			Eventually(session.Out).Should(Say(expectedMsg))
		})
	})

	Describe("if ssh responds to ps with evidence of pg_upgrade running", func() {
		It("reports that pg_upgrade is running on a host if it is", func() {
			grep_pg_upgrade := `
gpadmin            7520   0.0  0.0  2432772    676 s004  S+    3:56PM   0:00.00 grep pg_upgrade
pg_upgrade --verbose  --old-bindir /usr/local/greenplum-db-4.3.9.1/bin --new-bindir  /usr/local/greenplum-db-5/bin --old-datadir /data/gpdata/master/gpseg-1 --new-datadir /data/gp5data/master/gpseg-1 --old-port 5432 --new-port 6543 --link
`
			cheatSheet := CheatSheet{Response: grep_pg_upgrade, ReturnCode: intToBytes(0)}
			cheatSheet.WriteToFile()

			session := runCommand("monitor", "--host", "localhost", "--segment_id", "42", "--port", "2022", "--private_key", "sshd/private_key.pem", "--user", "pivotal")
			Eventually(session).Should(Exit(0))

			expectedMsg := "pg_upgrade is running on host localhost"
			Eventually(session.Out).Should(Say(expectedMsg))
		})
	})
	Describe("if it lacks host and segment ID", func() {
		It("complains", func() {
			session := runCommand("monitor")

			Eventually(session).Should(Exit(1))
			Eventually(session.Err).Should(Say("the required flags `--host' and `--segment_id' were not specified"))
		})
	})

	Describe("if a test run tries to use the default ssh key", func() {
		It("complains", func() {
			session := runCommand("monitor", "--host", "localhost", "--segment_id", "42")

			Eventually(session).Should(Exit(1))
			Eventually(session.Err).Should(Say("handshake failed"))
			Eventually(session.Err).Should(Say("unable to authenticate"))
		})
	})

	Describe("if ssh responds to ps with no pg_upgrade process", func() {
		It("connects and reports not running", func() {
			only_grep_itself := "gpadmin            7520   0.0  0.0  2432772    676 s004  S+    3:56PM   0:00.00 grep pg_upgrade"
			cheatSheet := CheatSheet{Response: only_grep_itself, ReturnCode: intToBytes(0)}
			cheatSheet.WriteToFile()

			session := runCommand("monitor", "--host", "localhost", "--segment_id", "42", "--port", "2022", "--private_key", "sshd/private_key.pem", "--user", "pivotal")
			Eventually(session).Should(Exit(0))

			expectedMsg := "pg_upgrade is not running on host localhost"
			Eventually(session.Out).Should(Say(expectedMsg))
		})
	})

	Describe("if the remote ssh command fails", func() {
		It("complains", func() {
			cheatSheet := CheatSheet{Response: "foo output", ReturnCode: intToBytes(1)}
			cheatSheet.WriteToFile()

			session := runCommand("monitor", "--host", "localhost", "--segment_id", "42", "--port", "2022", "--private_key", "sshd/private_key.pem", "--user", "pivotal")
			Eventually(session).Should(Exit(1))

			expectedMsg := "cannot run pgrep command on remote host, output: foo output\nError: Process exited with status 1"
			Eventually(session.Out).Should(Say(expectedMsg))
		})
	})
	Describe("if the private key cannot be ascertained", func() {
		It("complains", func() {
			save := os.Getenv("HOME")
			os.Setenv("HOME", "")

			session := runCommand("monitor", "--host", "localhost", "--segment_id", "42", "--port", "2022", "--private_key", "", "--user", "pivotal")
			os.Setenv("HOME", save)

			Eventually(session).Should(Exit(1))
			Eventually(session.Out).Should(Say("user has not specified a HOME environment value"))
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
