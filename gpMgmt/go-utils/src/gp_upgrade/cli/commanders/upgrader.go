package commanders

import (
	"context"

	pb "gp_upgrade/idl"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
)

type Upgrader struct {
	client pb.CliToHubClient
}

func NewUpgrader(client pb.CliToHubClient) *Upgrader {
	return &Upgrader{client: client}
}

func (u *Upgrader) ConvertMaster(oldDataDir string, oldBinDir string, newDataDir string, newBinDir string) error {
	upgradeConvertMasterRequest := pb.UpgradeConvertMasterRequest{
		OldDataDir: oldDataDir,
		OldBinDir:  oldBinDir,
		NewDataDir: newDataDir,
		NewBinDir:  newBinDir,
	}
	_, err := u.client.UpgradeConvertMaster(context.Background(), &upgradeConvertMasterRequest)
	if err != nil {
		// TODO: Change the logging message?
		gplog.Error("ERROR - Unable to connect to hub")
		return err
	}

	gplog.Info("Kicked off pg_upgrade request.")
	return nil
}

func (u *Upgrader) ConvertPrimaries(oldBinDir, newBinDir string) error {
	_, err := u.client.UpgradeConvertPrimaries(context.Background(), &pb.UpgradeConvertPrimariesRequest{
		OldBinDir: oldBinDir,
		NewBinDir: newBinDir,
	})
	if err != nil {
		// TODO: Change the logging message?
		gplog.Error("Error when calling hub upgrade convert primaries: %v", err.Error())
		return err
	}

	gplog.Info("Kicked off pg_upgrade request for primaries")
	return nil
}

func (u *Upgrader) ShareOids() error {
	_, err := u.client.UpgradeShareOids(context.Background(), &pb.UpgradeShareOidsRequest{})
	if err != nil {
		gplog.Error(err.Error())
		return err
	}

	gplog.Info("Kicked off request to share oids")
	return nil
}

func (u *Upgrader) ValidateStartCluster(newDataDir string, newBinDir string) error {
	_, err := u.client.UpgradeValidateStartCluster(context.Background(), &pb.UpgradeValidateStartClusterRequest{
		NewDataDir: newDataDir,
		NewBinDir:  newBinDir,
	})
	if err != nil {
		gplog.Error(err.Error())
		return err
	}

	gplog.Info("Kicked off request for validation of cluster startup")
	return nil
}

func (u *Upgrader) ReconfigurePorts() error {
	_, err := u.client.UpgradeReconfigurePorts(context.Background(), &pb.UpgradeReconfigurePortsRequest{})
	if err != nil {
		gplog.Error(err.Error())
		return err
	}

	gplog.Info("Request to reconfigure master port on upgraded cluster complete")
	return nil
}
