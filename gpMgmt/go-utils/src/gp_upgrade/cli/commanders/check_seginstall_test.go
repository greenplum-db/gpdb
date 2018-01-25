package commanders_test

import (
	"gp_upgrade/cli/commanders"

	"errors"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("CheckSeginstall", func() {
	var (
		spyClient  *spyCliToHubClient
		segChecker commanders.SeginstallChecker
	)

	BeforeEach(func() {
		spyClient = newSpyCliToHubClient()
		segChecker = commanders.NewSeginstallChecker(spyClient)
	})

	It("makes a CheckSeginstallRequest to the hub", func() {
		err := segChecker.Execute()
		Expect(spyClient.checkSeginstallCount).To(Equal(1))
		Expect(err).ToNot(HaveOccurred())
	})

	It("returns an error when CheckSeginstallRequest fails", func() {
		spyClient.err = errors.New("some error")
		err := segChecker.Execute()
		Expect(err).To(HaveOccurred())
	})
})
