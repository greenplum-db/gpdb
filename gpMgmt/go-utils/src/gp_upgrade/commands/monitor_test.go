package commands_test

import (
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gbytes"
	. "github.com/onsi/gomega/gexec"
)

var _ = Describe("monitor", func() {
	It("complains if it lacks host and segment ID", func() {
		session := runCommand("monitor")

		Eventually(session).Should(Exit(1))
		Eventually(session.Err).Should(Say("the required flags `--host' and `--segment_id' were not specified"))
	})

	It("connects to the host, yielding a session", func() {
		// ssh.tell_it_to("respond that pg_upgrade isn't running")

		session := runCommand("monitor", "--host", "localhost", "--segment_id", "42", "--port", "2022")
		Eventually(session).Should(Exit(0))

		// We would like to assert that connector was called -- can we spy on a mock of it at test runtime?
		Eventually(session.Out).Should(Say("pg_upgrade is not running on host 'localhost', segment_id '42'"))
	})
})
