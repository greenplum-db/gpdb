package services_test

import (
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"

	"gp_upgrade/hub/services"
)

var _ = Describe("ClusterSsher", func() {
	It("indicates that it is in progress on the hub filesystem", func() {
		cw := newSpyChecklistWriter()
		clusterSsher := services.NewClusterSsher(cw)
		clusterSsher.VerifySoftware([]string{"doesn't matter"})
		Expect(cw.inProgressCalls()).To(Equal(1))
		Expect(cw.stepRecorded()).To(Equal("seginstall"))
	})
})

func newSpyChecklistWriter() *spyChecklistWriter {
	return &spyChecklistWriter{
		numInProgressCalls: 0,
		stepRecord:         "",
	}
}

type spyChecklistWriter struct {
	numInProgressCalls int
	stepRecord         string
}

func (s *spyChecklistWriter) inProgressCalls() int {
	return s.numInProgressCalls
}

func (s *spyChecklistWriter) stepRecorded() string {
	return s.stepRecord
}

func (s *spyChecklistWriter) MarkInProgress(step string) {
	s.numInProgressCalls += 1
	s.stepRecord = step
}
