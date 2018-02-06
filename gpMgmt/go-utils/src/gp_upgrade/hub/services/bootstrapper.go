package services

import (
	pb "gp_upgrade/idl"

	"github.com/pkg/errors"

	"golang.org/x/net/context"
)

type Bootstrapper struct {
	hostnameGetter HostnameGetter
}

type HostnameGetter interface {
	GetHostnames() ([]string, error)
}

func NewBootstrapper(hg HostnameGetter) *Bootstrapper {
	return &Bootstrapper{
		hostnameGetter: hg,
	}
}

func (s *Bootstrapper) CheckSeginstall(ctx context.Context,
	in *pb.CheckSeginstallRequest) (*pb.CheckSeginstallReply, error) {
	//
	//gpbackupUtils.GetLogger().Info("starting CheckSeginstall()")
	//
	clusterHostnames, err := s.hostnameGetter.GetHostnames()
	if err != nil {
		return nil, err
	}
	if len(clusterHostnames) == 0 {
		return nil, errors.New("No cluster config found, did you forget to run gp_upgrade check config?")
	}

	successReply := &pb.CheckSeginstallReply{}
	return successReply, nil
}
