package configutils_test

import (
	"encoding/json"
	"gp_upgrade/testUtils"
	"os"

	"gp_upgrade/hub/configutils"
	"regexp"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("configutils reader", func() {

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
		subject        configutils.Reader
		json_structure []map[string]interface{}
	)

	BeforeEach(func() {
		saved_old_home = os.Getenv("HOME")
		testUtils.EnsureHomeDirIsTempAndClean()
		err := json.Unmarshal([]byte(expected_json), &json_structure)
		Expect(err).NotTo(HaveOccurred())
		subject = configutils.Reader{}
		subject.OfOldClusterConfig()
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
		It("returns an error if configutils cannot be read", func() {
			err := subject.Read()
			Expect(err).To(HaveOccurred())
		})
		It("returns list of hostnames", func() {
			testUtils.WriteSampleConfig()
			err := subject.Read()
			Expect(err).NotTo(HaveOccurred())
			Expect(subject.GetHostnames()).Should(ContainElement("briarwood"))
			Expect(subject.GetHostnames()).Should(ContainElement("aspen.pivotal"))
		})
		It("returns list of hostnames without duplicates", func() {
			re := regexp.MustCompile("aspen.pivotal")
			configWithDupe := re.ReplaceAllLiteralString(testUtils.SAMPLE_JSON, "briarwood")
			testUtils.WriteProvidedConfig(configWithDupe)
			err := subject.Read()
			Expect(err).NotTo(HaveOccurred())
			hostnames, err := subject.GetHostnames()
			Expect(len(hostnames)).Should(Equal(1))
			Expect(subject.GetHostnames()).Should(ContainElement("briarwood"))
		})
	})
})
