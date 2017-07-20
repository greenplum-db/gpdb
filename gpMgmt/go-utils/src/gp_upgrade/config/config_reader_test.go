package config_test

import (
	"encoding/json"
	"gp_upgrade/testUtils"
	"os"

	"gp_upgrade/config"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("config reader", func() {

	const (
		// the output is pretty-printed, so match that format precisely
		expected_json = `[
{
	"some": "json"
}
]`
	)

	var (
		saved_old_home string
		subject        config.Reader
		json_structure []map[string]interface{}
	)

	BeforeEach(func() {
		saved_old_home = testUtils.ResetTempHomeDir()
		err := json.Unmarshal([]byte(expected_json), &json_structure)
		Expect(err).NotTo(HaveOccurred())
		subject = config.Reader{}
	})

	AfterEach(func() {
		os.Setenv("HOME", saved_old_home)
	})

	Describe("#Read", func() {
		It("reads a configuration", func() {
			testUtils.WriteSampleConfig()
			err := subject.Read()

			Expect(err).NotTo(HaveOccurred())
			Expect(subject.GetPortForSegment(7)).ToNot(BeNil())
		})
		It("returns an error if config cannot be read", func() {
			err := subject.Read()
			Expect(err).To(HaveOccurred())
		})
	})
})
