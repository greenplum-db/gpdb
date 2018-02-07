package services_test

import (
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"

	"gp_upgrade/hub/logger"
	"gp_upgrade/hub/services"
	"gp_upgrade/utils"

	"github.com/pkg/errors"
)

var _ = Describe("ClusterSsher", func() {
	It("indicates that it is in progress, failed on the hub filesystem", func() {
		utils.System.ExecCmdOutput = func(name string, args ...string) ([]byte, error) {
			return nil, errors.New("host not found")
		}
		cw := newSpyChecklistWriter()
		//the buffer capacity is 100 to make sure that nothing blocks, there are no readers for this channel
		fakeErrors := make(chan string, 100)
		fakeLogger := logger.LogEntry{Error: fakeErrors}
		clusterSsher := services.NewClusterSsher(cw, fakeLogger)
		clusterSsher.VerifySoftware([]string{"doesnt matter"})
		Expect(cw.freshStateDirs).To(ContainElement("seginstall"))
		Expect(cw.stepsMarkedInProgress).To(ContainElement("seginstall"))
		Expect(cw.stepsMarkedFailed).To(ContainElement("seginstall"))
		Expect(cw.stepsMarkedCompleted).ToNot(ContainElement("seginstall"))
	})
	It("indicates that it is in progress, completed on the hub filesystem", func() {
		utils.System.ExecCmdOutput = func(name string, args ...string) ([]byte, error) {
			return []byte("completed"), nil
		}
		cw := newSpyChecklistWriter()
		//the buffer capacity is 100 to make sure that nothing blocks, there are no readers for this channel
		fakeErrors := make(chan string, 100)
		fakeLogger := logger.LogEntry{Error: fakeErrors}
		clusterSsher := services.NewClusterSsher(cw, fakeLogger)
		clusterSsher.VerifySoftware([]string{"doesnt matter"})
		Expect(cw.freshStateDirs).To(ContainElement("seginstall"))
		Expect(cw.stepsMarkedInProgress).To(ContainElement("seginstall"))
		Expect(cw.stepsMarkedFailed).ToNot(ContainElement("seginstall"))
		Expect(cw.stepsMarkedCompleted).To(ContainElement("seginstall"))
	})
})

func newSpyChecklistWriter() *spyChecklistWriter {
	return &spyChecklistWriter{}
}

type spyChecklistWriter struct {
	freshStateDirs        []string
	stepsMarkedInProgress []string
	stepsMarkedFailed     []string
	stepsMarkedCompleted  []string
}

func (s *spyChecklistWriter) MarkFailed(step string) error {
	s.stepsMarkedFailed = append(s.stepsMarkedFailed, step)
	return nil
}

func (s *spyChecklistWriter) MarkComplete(step string) error {
	s.stepsMarkedCompleted = append(s.stepsMarkedCompleted, step)
	return nil
}

func (s *spyChecklistWriter) MarkInProgress(step string) error {
	s.stepsMarkedInProgress = append(s.stepsMarkedInProgress, step)
	return nil
}

func (s *spyChecklistWriter) ResetStateDir(step string) error {
	s.freshStateDirs = append(s.freshStateDirs, step)
	return nil
}
