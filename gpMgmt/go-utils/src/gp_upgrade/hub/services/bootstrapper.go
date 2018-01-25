package services

import (
	pb "gp_upgrade/idl"

	"golang.org/x/net/context"
)

type Bootstrapper struct{}

func NewBootstrapper() *Bootstrapper {
	return &Bootstrapper{}
}

func (s *Bootstrapper) CheckSeginstall(ctx context.Context,
	in *pb.CheckSeginstallRequest) (*pb.CheckSeginstallReply, error) {
	//
	//gpbackupUtils.GetLogger().Info("starting CheckSeginstall()")
	//
	successReply := &pb.CheckSeginstallReply{}
	return successReply, nil
}
