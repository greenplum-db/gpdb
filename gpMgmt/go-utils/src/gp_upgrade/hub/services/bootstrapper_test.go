package services_test

import (
	_ "gp_upgrade/hub/services"

	"gp_upgrade/hub/services"
	pb "gp_upgrade/idl"

	"errors"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("HubCheckSeginstall", func() {
	It("returns a gRPC reply object, if the software verification gets underway asynch", func() {
		spyConfigReader := newSpyConfigReader()
		stubSoftwareVerifier := newStubSoftwareVerifier()
		bootstrapper := services.NewBootstrapper(spyConfigReader, stubSoftwareVerifier)
		spyConfigReader.failToGetHostnames = false

		_, err := bootstrapper.CheckSeginstall(nil, &pb.CheckSeginstallRequest{})
		Expect(err).ToNot(HaveOccurred())
		Eventually(stubSoftwareVerifier.hosts).Should(Receive(Equal([]string{"somehost"})))
	})

	It("returns an error if cluster config can't be read", func() {
		spyConfigReader := newSpyConfigReader()
		stubSoftwareVerifier := newStubSoftwareVerifier()
		bootstrapper := services.NewBootstrapper(spyConfigReader, stubSoftwareVerifier)
		spyConfigReader.failToGetHostnames = true

		_, err := bootstrapper.CheckSeginstall(nil, &pb.CheckSeginstallRequest{})
		Expect(err).To(HaveOccurred())
	})

	It("returns an error if cluster config is empty", func() {
		spyConfigReader := newSpyConfigReader()
		stubSoftwareVerifier := newStubSoftwareVerifier()
		bootstrapper := services.NewBootstrapper(spyConfigReader, stubSoftwareVerifier)
		spyConfigReader.failToGetHostnames = false
		spyConfigReader.hostnamesListEmpty = true

		_, err := bootstrapper.CheckSeginstall(nil, &pb.CheckSeginstallRequest{})
		Expect(err).To(HaveOccurred())
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

type stubSoftwareVerifier struct {
	hosts chan []string
}

func newStubSoftwareVerifier() *stubSoftwareVerifier {
	return &stubSoftwareVerifier{
		hosts: make(chan []string),
	}
}

func (s *stubSoftwareVerifier) VerifySoftware(hosts []string) {
	s.hosts <- hosts
}
