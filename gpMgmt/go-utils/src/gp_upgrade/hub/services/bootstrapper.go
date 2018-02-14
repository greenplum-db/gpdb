package services

import (
	pb "gp_upgrade/idl"

	"github.com/pkg/errors"

	"golang.org/x/net/context"
)

type Bootstrapper struct {
	hostnameGetter HostnameGetter
	remoteExecutor RemoteExecutor
}

type HostnameGetter interface {
	GetHostnames() ([]string, error)
}

type RemoteExecutor interface {
	VerifySoftware(hosts []string)
	Start(hosts []string)
}

func NewBootstrapper(hg HostnameGetter, r RemoteExecutor) *Bootstrapper {
	return &Bootstrapper{
		hostnameGetter: hg,
		remoteExecutor: r,
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

	go s.remoteExecutor.VerifySoftware(clusterHostnames)

	return &pb.CheckSeginstallReply{}, nil
}

func (s *Bootstrapper) PrepareStartAgents(ctx context.Context,
	in *pb.PrepareStartAgentsRequest) (*pb.PrepareStartAgentsReply, error) {

	clusterHostnames, err := s.hostnameGetter.GetHostnames()
	if err != nil || len(clusterHostnames) == 0 {
		return nil, errors.New("No cluster config found, did you forget to run gp_upgrade check config?")
	}

	go s.remoteExecutor.Start(clusterHostnames)

	return &pb.PrepareStartAgentsReply{}, nil
}
