package commanders_test

import (
	pb "gp_upgrade/idl"

	"golang.org/x/net/context"
	"google.golang.org/grpc"
)

type spyCliToHubClient struct {
	pb.CliToHubClient

	checkSeginstallCount int

	statusUpgradeCount int
	statusUpgradeReply *pb.StatusUpgradeReply

	err error
}

func newSpyCliToHubClient() *spyCliToHubClient {
	return &spyCliToHubClient{
		statusUpgradeReply: &pb.StatusUpgradeReply{},
	}
}

func (s *spyCliToHubClient) CheckSeginstall(
	ctx context.Context,
	request *pb.CheckSeginstallRequest,
	opts ...grpc.CallOption,
) (*pb.CheckSeginstallReply, error) {

	s.checkSeginstallCount++
	return &pb.CheckSeginstallReply{}, s.err
}

func (s *spyCliToHubClient) StatusUpgrade(
	ctx context.Context,
	request *pb.StatusUpgradeRequest,
	opts ...grpc.CallOption,
) (*pb.StatusUpgradeReply, error) {

	s.statusUpgradeCount++
	return s.statusUpgradeReply, s.err
}
