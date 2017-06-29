package integrations_test

import (
	"fmt"
	"os/exec"
	"time"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	. "github.com/onsi/gomega/gbytes"
	. "github.com/onsi/gomega/gexec"
)

var _ = Describe("version command", func() {
	It("reports the version that's injected at build-time", func() {
		fake_version := fmt.Sprintf("v0.0.0-dev.%d", time.Now().Unix())
		commandPathWithVersion, err := Build("gp_upgrade", "-ldflags", "-X gp_upgrade/commands.GpdbVersion="+fake_version)
		Expect(err).NotTo(HaveOccurred())

		// can't use the runCommand() integration helper function because we calculated a separate path
		cmd := exec.Command(commandPathWithVersion, "version")
		session, err := Start(cmd, GinkgoWriter, GinkgoWriter)
		Expect(err).NotTo(HaveOccurred())

		Eventually(session).Should(Exit(0))
		Consistently(session.Out).ShouldNot(Say("unknown version"))
		Eventually(session.Out).Should(Say("gp_upgrade version")) //scans session.Out buffer beyond the matching tokens
		Eventually(session.Out).Should(Say(fake_version))
	})
})
