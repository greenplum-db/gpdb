package config_test

import (
	"gp_upgrade/test_utils"
	"os"

	"gp_upgrade/config"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("config reader", func() {

	var (
		saved_old_home string
	)

	BeforeEach(func() {
		saved_old_home = test_utils.ResetTempHomeDir()
	})

	AfterEach(func() {
		os.Setenv("HOME", saved_old_home)
	})

	Describe("#Read", func() {
		BeforeEach(func() {
			test_utils.WriteSampleConfig()
		})
		It("reads a configuration", func() {
			subject := config.Reader{}
			err := subject.Read()

			Expect(err).NotTo(HaveOccurred())
			Expect(subject.GetPortForSegment(7)).ToNot(BeNil())
		})
		//Describe("error cases", func() {
		//	It("returns an error when home directory is not writable", func() {
		//		os.Chmod(test_utils.TempHomeDir, 0100)
		//		subject := config.Writer{
		//			TableJsonData: json_structure,
		//			Formatter:     config.NewJsonFormatter(),
		//			FileWriter:    config.NewRealFileWriter(),
		//		}
		//		err := subject.Write()
		//
		//		Expect(err).To(HaveOccurred())
		//		Expect(string(err.Error())).To(ContainSubstring(fmt.Sprintf("mkdir %v/.gp_upgrade: permission denied", test_utils.TempHomeDir)))
		//	})
		//})
	})
})
