package commands_test

import (
	"gp_upgrade/commands"

	"os"

	"bytes"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("PrivateKeyGuarantor", func() {

	Describe("#Check", func() {
		Context("user has specified a private key option", func() {
			It("returns that specified key path", func() {
				guarantor := commands.NewPrivateKeyGuarantor()
				value := guarantor.Check("foo")
				Expect(value).To(Equal("foo"))
			})
		})

		Context("user has not specified a key", func() {
			It("returns the path to user's home dir + /.ssh/id_rsa", func() {
				os.Setenv("HOME", "/foo")

				guarantor := commands.NewPrivateKeyGuarantor()
				value := guarantor.Check("")
				Expect(value).To(Equal("/foo/.ssh/id_rsa"))
			})
		})

		Context("user has not specified a key and has no HOME environment variable setting", func() {
			It("logs fatally", func() {
				buf := &bytes.Buffer{}
				commands.Stdout = buf

				guarantor := commands.NewPrivateKeyGuarantor()
				guarantor.Check("")

				Expect(string(buf.Bytes())).To(Equal("environmental variable 'HOME' not set"))
			})
		})
	})

})