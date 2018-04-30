package services

import (
	"context"
	"fmt"
	"gp_upgrade/hub/upgradestatus"
	pb "gp_upgrade/idl"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
)

const (
	SedAndMvString = "sed 's/port=%d/port=%d/' %[3]s/postgresql.conf > %[3]s/postgresql.conf.updated && " +
		"mv %[3]s/postgresql.conf %[3]s/postgresql.conf.bak && " +
		"mv %[3]s/postgresql.conf.updated %[3]s/postgresql.conf"
)

func (h *HubClient) UpgradeReconfigurePorts(ctx context.Context, in *pb.UpgradeReconfigurePortsRequest) (*pb.UpgradeReconfigurePortsReply, error) {
	gplog.Info("Started processing reconfigure-ports request")

	c := upgradestatus.NewChecklistManager(h.conf.StateDir)
	reconfigurePortsStep := "reconfigure-ports"

	err := c.ResetStateDir(reconfigurePortsStep)
	if err != nil {
		gplog.Error("error from ResetStateDir " + err.Error())
	}
	err = c.MarkInProgress(reconfigurePortsStep)
	if err != nil {
		gplog.Error("error from MarkInProgress " + err.Error())
	}

	oldMasterPort, newMasterPort, newMasterDataDir := h.clusterPair.GetPortsAndDataDirForReconfiguration()
	sedCommand := h.commandExecer("bash", "-c", fmt.Sprintf(SedAndMvString, newMasterPort, oldMasterPort, newMasterDataDir))
	gplog.Info("reconfigure-ports sed command: %+v", sedCommand)

	output, err := sedCommand.CombinedOutput()
	if err != nil {
		var out string
		if len(output) != 0 {
			out = string(output)
		}
		gplog.Error("reconfigure-ports failed %s: %s", out, err)

		c.MarkFailed(reconfigurePortsStep)
		return nil, err
	}

	gplog.Info("reconfigure-ports succeeded")
	c.MarkComplete(reconfigurePortsStep)

	return &pb.UpgradeReconfigurePortsReply{}, nil
}
