package commanders

import (
	"context"
	pb "gp_upgrade/idl"

	gpbackupUtils "github.com/greenplum-db/gp-common-go-libs/gplog"
)

type DiskUsageChecker struct {
	client pb.CliToHubClient
}

func NewDiskUsageChecker(client pb.CliToHubClient) DiskUsageChecker {
	return DiskUsageChecker{client: client}
}

func (req DiskUsageChecker) Execute(dbPort int) error {
	reply, err := req.client.CheckDiskUsage(context.Background(),
		&pb.CheckDiskUsageRequest{})
	if err != nil {
		gpbackupUtils.Error("ERROR - gRPC call to hub failed")
		return err
	}

	//TODO: do we want to report results to the user earlier? Should we make a gRPC call per db?
	for _, segmentFileSysUsage := range reply.SegmentFileSysUsage {
		gpbackupUtils.Info(segmentFileSysUsage)
	}
	gpbackupUtils.Info("Check disk space request is processed.")
	return nil
}
