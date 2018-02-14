package services_test

import (
	_ "gp_upgrade/hub/services"

	"gp_upgrade/hub/services"
	pb "gp_upgrade/idl"

	"errors"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("Bootstrapper", func() {
	var (
		spyConfigReader    *spyConfigReader
		stubRemoteExecutor *stubRemoteExecutor
		bootstrapper       *services.Bootstrapper
	)
	BeforeEach(func() {
		spyConfigReader = newSpyConfigReader()
		stubRemoteExecutor = newStubRemoteExecutor()
		bootstrapper = services.NewBootstrapper(spyConfigReader, stubRemoteExecutor)
	})
	Describe("CheckSeginstall", func() {
		It("returns a gRPC reply object, if the software verification gets underway asynch", func() {
			spyConfigReader.failToGetHostnames = false

			_, err := bootstrapper.CheckSeginstall(nil, &pb.CheckSeginstallRequest{})
			Expect(err).ToNot(HaveOccurred())
			Eventually(stubRemoteExecutor.verifySoftwareHosts).Should(Receive(Equal([]string{"somehost"})))
		})

		It("returns an error if cluster config can't be read", func() {
			spyConfigReader.failToGetHostnames = true

			_, err := bootstrapper.CheckSeginstall(nil, &pb.CheckSeginstallRequest{})
			Expect(err).To(HaveOccurred())
		})

		It("returns an error if cluster config is empty", func() {
			spyConfigReader.failToGetHostnames = false
			spyConfigReader.hostnamesListEmpty = true

			_, err := bootstrapper.CheckSeginstall(nil, &pb.CheckSeginstallRequest{})
			Expect(err).To(HaveOccurred())
		})
	})

	Describe("PrepareStartAgents", func() {
		It("returns a gRPC object", func() {
			reply, err := bootstrapper.PrepareStartAgents(nil, nil)
			Expect(err).ToNot(HaveOccurred())
			Expect(reply).ToNot(BeNil())
			Eventually(stubRemoteExecutor.startHosts).Should(Receive(Equal([]string{"somehost"})))
		})
		It("returns an error if cluster config can't be read", func() {
			spyConfigReader.failToGetHostnames = true

			_, err := bootstrapper.PrepareStartAgents(nil, &pb.PrepareStartAgentsRequest{})
			Expect(err).To(HaveOccurred())
		})
		It("returns an error if cluster config is empty", func() {
			spyConfigReader.failToGetHostnames = false
			spyConfigReader.hostnamesListEmpty = true

			_, err := bootstrapper.PrepareStartAgents(nil, &pb.PrepareStartAgentsRequest{})
			Expect(err).To(HaveOccurred())
		})
	})

})

type spyConfigReader struct {
	failToGetHostnames bool
	hostnamesListEmpty bool
	//
	//hostnames []string
	//err error
}

func newSpyConfigReader() *spyConfigReader { return &spyConfigReader{} }

func (scr *spyConfigReader) GetHostnames() ([]string, error) {
	//return scr.hostnames, scr.err

	if scr.failToGetHostnames {
		return nil, errors.New("force failure - no config")
	}
	if scr.hostnamesListEmpty == true {
		return []string{}, nil
	} else {
		return []string{"somehost"}, nil
	}
}

type stubRemoteExecutor struct {
	verifySoftwareHosts chan []string
	startHosts          chan []string
}

func newStubRemoteExecutor() *stubRemoteExecutor {
	return &stubRemoteExecutor{
		verifySoftwareHosts: make(chan []string),
		startHosts:          make(chan []string),
	}
}

func (s *stubRemoteExecutor) VerifySoftware(hosts []string) {
	s.verifySoftwareHosts <- hosts
}

func (a *stubRemoteExecutor) Start(hosts []string) {
	a.startHosts <- hosts
}
