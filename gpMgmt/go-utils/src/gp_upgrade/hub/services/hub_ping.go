package services

import (
	pb "gp_upgrade/idl"

	gpbackupUtils "github.com/greenplum-db/gp-common-go-libs/gplog"
	"golang.org/x/net/context"
)

func (s *CatchAllCliToHubListenerImpl) Ping(ctx context.Context,
	in *pb.PingRequest) (*pb.PingReply, error) {

	gpbackupUtils.Info("starting Ping")
	return &pb.PingReply{}, nil
}
