package commands

import (
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("Version", func() {
	Describe("VersionString", func() {
		Context("when global var GpdbVersion is the empty string", func() {
			It("returns the default version", func() {
				GpdbVersion = ""
				Expect(versionString()).To(Equal("gp_upgrade unknown version"))
			})
		})

		Context("when global var GpdbVersion is set to something", func() {
			It("returns what it's set to", func() {
				GpdbVersion = "SomEthing"
				Expect(versionString()).To(Equal("gp_upgrade version SomEthing"))
			})
		})
	})
})
