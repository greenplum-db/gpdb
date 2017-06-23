package integrations_test

import (
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gbytes"
	. "github.com/onsi/gomega/gexec"
)

var _ = Describe("all", func() {
	It("handles no params call by outputting help text", func() {
		session := runCommand()

		Eventually(session).Should(Exit(1))
		Eventually(session.Err).Should(Say("Please specify one command of: check, monitor or version"))
	})
})
