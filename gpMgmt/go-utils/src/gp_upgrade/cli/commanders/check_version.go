package commanders

import (
	"context"
	pb "gp_upgrade/idl"

	gpbackupUtils "github.com/greenplum-db/gp-common-go-libs/gplog"
)

type VersionChecker struct {
	client pb.CliToHubClient
}

func NewVersionChecker(client pb.CliToHubClient) VersionChecker {
	return VersionChecker{
		client: client,
	}
}

func (req VersionChecker) Execute(masterHost string, dbPort int) error {
	resp, err := req.client.CheckVersion(context.Background(),
		&pb.CheckVersionRequest{Host: masterHost, DbPort: int32(dbPort)})
	if err != nil {
		gpbackupUtils.Error("ERROR - gRPC call to hub failed")
		return err
	}
	if resp.IsVersionCompatible {
		gpbackupUtils.Info("gp_upgrade: Version Compatibility Check [OK]\n")
	} else {
		gpbackupUtils.Info("gp_upgrade: Version Compatibility Check [Failed]\n")
	}
	gpbackupUtils.Info("Check version request is processed.")

	return nil
}
