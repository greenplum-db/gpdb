package services_test

import (
	_ "gp_upgrade/hub/services"

	"gp_upgrade/hub/services"
	pb "gp_upgrade/idl"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("HubCheckSeginstall", func() {
	It("returns a gRPC reply object", func() {
		bootstrapper := services.NewBootstrapper()
		_, err := bootstrapper.CheckSeginstall(nil, &pb.CheckSeginstallRequest{})
		Expect(err).ToNot(HaveOccurred())
	})
})
