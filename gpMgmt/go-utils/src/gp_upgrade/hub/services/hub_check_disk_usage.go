package services

import (
	"fmt"
	"gp_upgrade/hub/configutils"
	pb "gp_upgrade/idl"

	gpbackupUtils "github.com/greenplum-db/gp-common-go-libs/gplog"
	"golang.org/x/net/context"
	"google.golang.org/grpc"
)

const (
	// todo generalize to any host
	address               = "localhost"
	port                  = "6416"
	diskUsageWarningLimit = 80
)

func (s *CatchAllCliToHubListenerImpl) CheckDiskUsage(ctx context.Context,
	in *pb.CheckDiskUsageRequest) (*pb.CheckDiskUsageReply, error) {

	gpbackupUtils.Info("starting CheckDiskUsage")
	var replyMessages []string
	reader := configutils.Reader{}
	// We don't care whether this the old json vs the new json because we're
	// just checking the hosts anyways.
	reader.OfOldClusterConfig()
	hostnames, err := reader.GetHostnames()
	if err != nil {
		return nil, err
	}
	var clients []configutils.ClientAndHostname
	for i := 0; i < len(hostnames); i++ {
		conn, err := grpc.Dial(hostnames[i]+":"+port, grpc.WithInsecure())
		if err == nil {
			clients = append(clients, configutils.ClientAndHostname{Client: pb.NewCommandListenerClient(conn), Hostname: hostnames[i]})
			defer conn.Close()
		} else {
			gpbackupUtils.Error(err.Error())
			replyMessages = append(replyMessages, "ERROR: couldn't get gRPC conn to "+hostnames[i])
		}
	}
	replyMessages = append(replyMessages, GetDiskUsageFromSegmentHosts(clients)...)

	return &pb.CheckDiskUsageReply{SegmentFileSysUsage: replyMessages}, nil
}

func GetDiskUsageFromSegmentHosts(clients []configutils.ClientAndHostname) []string {
	replyMessages := []string{}
	for i := 0; i < len(clients); i++ {
		reply, err := clients[i].Client.CheckDiskUsageOnAgents(context.Background(),
			&pb.CheckDiskUsageRequestToAgent{})
		if err != nil {
			gpbackupUtils.Error(err.Error())
			replyMessages = append(replyMessages, "Could not get disk usage from: "+clients[i].Hostname)
			continue
		}
		foundAnyTooFull := false
		for _, line := range reply.ListOfFileSysUsage {
			if line.Usage >= diskUsageWarningLimit {
				replyMessages = append(replyMessages, fmt.Sprintf("diskspace check - %s - WARNING %s %.1f use",
					clients[i].Hostname, line.Filesystem, line.Usage))
				foundAnyTooFull = true
			}
		}
		if !foundAnyTooFull {
			replyMessages = append(replyMessages, fmt.Sprintf("diskspace check - %s - OK", clients[i].Hostname))
		}
	}

	return replyMessages
}
