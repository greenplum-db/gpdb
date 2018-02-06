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
	It("returns a gRPC reply object", func() {
		spyConfigReader := newSpyConfigReader()
		bootstrapper := services.NewBootstrapper(spyConfigReader)
		spyConfigReader.failToGetHostnames = false

		_, err := bootstrapper.CheckSeginstall(nil, &pb.CheckSeginstallRequest{})
		Expect(err).ToNot(HaveOccurred())
	})

	It("returns an error if cluster config can't be read", func() {
		spyConfigReader := newSpyConfigReader()
		bootstrapper := services.NewBootstrapper(spyConfigReader)
		spyConfigReader.failToGetHostnames = true

		_, err := bootstrapper.CheckSeginstall(nil, &pb.CheckSeginstallRequest{})
		Expect(err).To(HaveOccurred())
	})
	It("returns an error if cluster config is empty", func() {
		spyConfigReader := newSpyConfigReader()
		bootstrapper := services.NewBootstrapper(spyConfigReader)
		spyConfigReader.failToGetHostnames = false
		spyConfigReader.hostnamesListEmpty = true

		_, err := bootstrapper.CheckSeginstall(nil, &pb.CheckSeginstallRequest{})
		Expect(err).To(HaveOccurred())
	})
})

type spyConfigReader struct {
	failToGetHostnames bool
	hostnamesListEmpty bool
}

func newSpyConfigReader() *spyConfigReader { return &spyConfigReader{} }

func (scr *spyConfigReader) GetHostnames() ([]string, error) {
	if scr.failToGetHostnames {
		return nil, errors.New("force failure - no config")
	}
	if scr.hostnamesListEmpty == true {
		return []string{}, nil
	} else {
		return []string{"somehost"}, nil
	}
}
