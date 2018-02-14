package commanders

import (
	"context"

	pb "gp_upgrade/idl"

	gpbackupUtils "github.com/greenplum-db/gp-common-go-libs/gplog"
)

type Upgrader struct {
	client pb.CliToHubClient
}

func NewUpgrader(client pb.CliToHubClient) Upgrader {
	return Upgrader{client: client}
}

func (u Upgrader) ConvertMaster(oldDataDir string, oldBinDir string, newDataDir string, newBinDir string) error {
	upgradeConvertMasterRequest := pb.UpgradeConvertMasterRequest{
		OldDataDir: oldDataDir,
		OldBinDir:  oldBinDir,
		NewDataDir: newDataDir,
		NewBinDir:  newBinDir,
	}
	_, err := u.client.UpgradeConvertMaster(context.Background(), &upgradeConvertMasterRequest)
	if err != nil {
		// TODO: Change the logging message?
		gpbackupUtils.Error("ERROR - Unable to connect to hub")
		return err
	}

	gpbackupUtils.Info("Kicked off pg_upgrade request.")
	return nil
}
