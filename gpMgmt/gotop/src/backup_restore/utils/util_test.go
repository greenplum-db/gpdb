package utils_test

import (
	"backup_restore/utils"
	"testing"
	"time"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

func TestUtil(t *testing.T) {
	RegisterFailHandler(Fail)
	RunSpecs(t, "utils tests")
}

var _ = Describe("utils/util tests", func() {
	Context("CurrentTimestamp", func() {
		It("returns the current timestamp", func() {
			utils.FPTimeNow = func() time.Time { return time.Date(2017, time.January, 1, 1, 1, 1, 1, time.Local) }
			expected := "20170101010101"
			actual := utils.CurrentTimestamp()
			Expect(actual).To(Equal(expected))
		})
	})
})
