package commanders

import (
	"context"
	pb "gp_upgrade/idl"

	gpbackupUtils "github.com/greenplum-db/gp-common-go-libs/gplog"
)

type ConfigChecker struct {
	client pb.CliToHubClient
}

func NewConfigChecker(client pb.CliToHubClient) ConfigChecker {
	return ConfigChecker{
		client: client,
	}
}

func (req ConfigChecker) Execute(dbPort int) error {
	_, err := req.client.CheckConfig(context.Background(),
		&pb.CheckConfigRequest{DbPort: int32(dbPort)})
	if err != nil {
		gpbackupUtils.Error("ERROR - gRPC call to hub failed")
		return err
	}
	gpbackupUtils.Info("Check config request is processed.")
	return nil
}
