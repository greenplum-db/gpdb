package commanders

import (
	"context"
	"fmt"
	pb "gp_upgrade/idl"

	"github.com/pkg/errors"
)

type Reporter struct {
	client pb.CliToHubClient
	logger Logger
}

// UpgradeStepsMessage encode the proper checklist item string to go with a step
//
// Future steps include:
//logger.Info("PENDING - Validate compatible versions for upgrade")
//logger.Info("PENDING - Master server upgrade")
//logger.Info("PENDING - Master OID file shared with segments")
//logger.Info("PENDING - Primary segment upgrade")
//logger.Info("PENDING - Validate cluster start")
//logger.Info("PENDING - Adjust upgrade cluster ports")
var UpgradeStepsMessage = map[pb.UpgradeSteps]string{
	pb.UpgradeSteps_UNKNOWN_STEP:         "- Unknown step",
	pb.UpgradeSteps_CHECK_CONFIG:         "- Configuration Check",
	pb.UpgradeSteps_SEGINSTALL:           "- Install binaries on segments",
	pb.UpgradeSteps_PREPARE_INIT_CLUSTER: "- Initialize upgrade target cluster",
	pb.UpgradeSteps_MASTERUPGRADE:        "- Run pg_upgrade on master",
	pb.UpgradeSteps_STOPPED_CLUSTER:      "- Shutdown clusters",
}

func NewReporter(client pb.CliToHubClient, logger Logger) *Reporter {
	return &Reporter{
		client: client,
		logger: logger,
	}
}

func (r *Reporter) OverallUpgradeStatus() error {
	reply, err := r.client.StatusUpgrade(context.Background(), &pb.StatusUpgradeRequest{})
	if err != nil {
		// find some way to expound on the error message? Integration test failing because we no longer log here
		return errors.New("Unable to connect to hub: " + err.Error())
	}

	for i := 0; i < len(reply.ListOfUpgradeStepStatuses); i++ {
		upgradeStepStatus := reply.ListOfUpgradeStepStatuses[i]
		reportString := fmt.Sprintf("%v %s", upgradeStepStatus.Status,
			UpgradeStepsMessage[upgradeStepStatus.Step])
		r.logger.Info(reportString)
	}

	return nil
}

type Logger interface {
	Info(string, ...interface{})
}
