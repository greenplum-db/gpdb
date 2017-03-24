package ssh_client_test

import (
	"os"

	"gp_upgrade/ssh_client"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("PrivateKeyGuarantor", func() {
	var (
		subject *ssh_client.PrivateKeyGuarantor
	)

	BeforeEach(func() {
		subject = ssh_client.NewPrivateKeyGuarantor()
	})

	Describe("#Check", func() {
		Context("user has specified a private key option", func() {
			It("returns that specified key path", func() {
				value, _ := subject.Check("foo")
				Expect(value).To(Equal("foo"))
			})
		})

		Context("user has specified a private key option with tilde", func() {
			It("returns that expanded key path", func() {
				value, _ := subject.Check("~/foo")
				home := os.Getenv("HOME")

				Expect(value).To(Equal(home + "/foo"))
			})
		})

		Context("user has not specified a key", func() {
			It("returns the path to user's home dir + /.ssh/id_rsa", func() {
				save := os.Getenv("HOME")
				os.Setenv("HOME", "/foo")

				value, _ := subject.Check("")

				os.Setenv("HOME", save)

				Expect(value).To(Equal("/foo/.ssh/id_rsa"))
			})
		})

		Context("user has not specified a key and has no HOME environment variable setting", func() {
			It("returns an error", func() {
				save := os.Getenv("HOME")
				os.Setenv("HOME", "")

				_, err := subject.Check("")
				os.Setenv("HOME", save)

				Expect(err).To(HaveOccurred())
				Expect(err.Error()).To(Equal("user has not specified a HOME environment value"))
			})
		})
	})

})
