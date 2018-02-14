package commanders

import (
	"context"
	pb "gp_upgrade/idl"

	gpbackupUtils "github.com/greenplum-db/gp-common-go-libs/gplog"
)

type ObjectCountChecker struct {
	client pb.CliToHubClient
}

func NewObjectCountChecker(client pb.CliToHubClient) ObjectCountChecker {
	return ObjectCountChecker{client: client}
}

func (req ObjectCountChecker) Execute(dbPort int) error {
	reply, err := req.client.CheckObjectCount(context.Background(),
		&pb.CheckObjectCountRequest{DbPort: int32(dbPort)})
	if err != nil {
		gpbackupUtils.Error("ERROR - gRPC call to hub failed")
		return err
	}
	//TODO: do we want to report results to the user earlier? Should we make a gRPC call per db?
	for _, count := range reply.ListOfCounts {
		gpbackupUtils.Info("Checking object counts in database: %s", count.DbName)
		gpbackupUtils.Info("Number of AO objects - %d", count.AoCount)
		gpbackupUtils.Info("Number of heap objects - %d", count.HeapCount)
	}
	gpbackupUtils.Info("Check object count request is processed.")
	return nil
}
