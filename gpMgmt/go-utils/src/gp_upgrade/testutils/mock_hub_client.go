package testutils

import (
	"context"

	pb "gp_upgrade/idl"

	"google.golang.org/grpc"
)

type MockHubClient struct {
	UpgradeShareOidsRequest        *pb.UpgradeShareOidsRequest
	UpgradeReconfigurePortsRequest *pb.UpgradeReconfigurePortsRequest

	UpgradeConvertPrimariesRequest  *pb.UpgradeConvertPrimariesRequest
	UpgradeConvertPrimariesResponse *pb.UpgradeConvertPrimariesReply
	Err                             error
}

func NewMockHubClient() *MockHubClient {
	return &MockHubClient{}
}

func (m *MockHubClient) Ping(ctx context.Context, in *pb.PingRequest, opts ...grpc.CallOption) (*pb.PingReply, error) {
	return nil, nil
}

func (m *MockHubClient) StatusUpgrade(ctx context.Context, in *pb.StatusUpgradeRequest, opts ...grpc.CallOption) (*pb.StatusUpgradeReply, error) {
	return nil, nil
}

func (m *MockHubClient) StatusConversion(ctx context.Context, in *pb.StatusConversionRequest, opts ...grpc.CallOption) (*pb.StatusConversionReply, error) {
	return nil, nil
}

func (m *MockHubClient) CheckConfig(ctx context.Context, in *pb.CheckConfigRequest, opts ...grpc.CallOption) (*pb.CheckConfigReply, error) {
	return nil, nil
}

func (m *MockHubClient) CheckSeginstall(ctx context.Context, in *pb.CheckSeginstallRequest, opts ...grpc.CallOption) (*pb.CheckSeginstallReply, error) {
	return nil, nil
}

func (m *MockHubClient) CheckObjectCount(ctx context.Context, in *pb.CheckObjectCountRequest, opts ...grpc.CallOption) (*pb.CheckObjectCountReply, error) {
	return nil, nil
}

func (m *MockHubClient) CheckVersion(ctx context.Context, in *pb.CheckVersionRequest, opts ...grpc.CallOption) (*pb.CheckVersionReply, error) {
	return nil, nil
}

func (m *MockHubClient) CheckDiskUsage(ctx context.Context, in *pb.CheckDiskUsageRequest, opts ...grpc.CallOption) (*pb.CheckDiskUsageReply, error) {
	return nil, nil
}

func (m *MockHubClient) PrepareInitCluster(ctx context.Context, in *pb.PrepareInitClusterRequest, opts ...grpc.CallOption) (*pb.PrepareInitClusterReply, error) {
	return nil, nil
}

func (m *MockHubClient) PrepareShutdownClusters(ctx context.Context, in *pb.PrepareShutdownClustersRequest, opts ...grpc.CallOption) (*pb.PrepareShutdownClustersReply, error) {
	return nil, nil
}

func (m *MockHubClient) UpgradeConvertMaster(ctx context.Context, in *pb.UpgradeConvertMasterRequest, opts ...grpc.CallOption) (*pb.UpgradeConvertMasterReply, error) {
	return nil, nil
}

func (m *MockHubClient) PrepareStartAgents(ctx context.Context, in *pb.PrepareStartAgentsRequest, opts ...grpc.CallOption) (*pb.PrepareStartAgentsReply, error) {
	return nil, nil
}

func (m *MockHubClient) UpgradeShareOids(ctx context.Context, in *pb.UpgradeShareOidsRequest, opts ...grpc.CallOption) (*pb.UpgradeShareOidsReply, error) {
	m.UpgradeShareOidsRequest = in

	return &pb.UpgradeShareOidsReply{}, m.Err
}

func (m *MockHubClient) UpgradeValidateStartCluster(ctx context.Context, in *pb.UpgradeValidateStartClusterRequest, opts ...grpc.CallOption) (*pb.UpgradeValidateStartClusterReply, error) {
	return nil, nil
}

func (m *MockHubClient) UpgradeConvertPrimaries(ctx context.Context, in *pb.UpgradeConvertPrimariesRequest, opts ...grpc.CallOption) (*pb.UpgradeConvertPrimariesReply, error) {
	m.UpgradeConvertPrimariesRequest = in

	return m.UpgradeConvertPrimariesResponse, m.Err
}

func (m *MockHubClient) UpgradeReconfigurePorts(ctx context.Context, in *pb.UpgradeReconfigurePortsRequest, opts ...grpc.CallOption) (*pb.UpgradeReconfigurePortsReply, error) {
	m.UpgradeReconfigurePortsRequest = in

	return nil, m.Err
}
