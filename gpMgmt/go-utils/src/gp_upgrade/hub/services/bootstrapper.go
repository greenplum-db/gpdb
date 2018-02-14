package services

import (
	pb "gp_upgrade/idl"

	"github.com/pkg/errors"

	"golang.org/x/net/context"
)

type Bootstrapper struct {
	hostnameGetter   HostnameGetter
	softwareVerifier SoftwareVerifier
}

type HostnameGetter interface {
	GetHostnames() ([]string, error)
}

type SoftwareVerifier interface {
	VerifySoftware(hosts []string)
}

func NewBootstrapper(hg HostnameGetter, s SoftwareVerifier) *Bootstrapper {
	return &Bootstrapper{
		hostnameGetter:   hg,
		softwareVerifier: s,
	}
}

func (s *Bootstrapper) CheckSeginstall(ctx context.Context,
	in *pb.CheckSeginstallRequest) (*pb.CheckSeginstallReply, error) {
	//
	//gpbackupUtils.Info("starting CheckSeginstall()")
	//
	clusterHostnames, err := s.hostnameGetter.GetHostnames()
	if err != nil || len(clusterHostnames) == 0 {
		return nil, errors.New("No cluster config found, did you forget to run gp_upgrade check config?")
	}

	go s.softwareVerifier.VerifySoftware(clusterHostnames)

	successReply := &pb.CheckSeginstallReply{}
	return successReply, nil
}
