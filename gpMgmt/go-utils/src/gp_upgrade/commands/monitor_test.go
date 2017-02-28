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
})
