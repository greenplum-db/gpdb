package services

import (
	"gp_upgrade/hub/cluster"
	"gp_upgrade/hub/configutils"
	"gp_upgrade/hub/logger"
	"gp_upgrade/hub/upgradestatus"
	"os"
	"path/filepath"
)

func NewCliToHubListener(logger logger.LogEntry, pair cluster.PairOperator) *CliToHubListenerImpl {
	impl := &CliToHubListenerImpl{}
	impl.CatchAllCliToHubListenerImpl.logger = logger
	impl.CatchAllCliToHubListenerImpl.clusterPair = pair
	configReader := configutils.NewReader()
	// refactor opportunity -- don't use this pattern,
	// use different types or separate functions for old/new or set the config path at reader initialization time
	configReader.OfOldClusterConfig()
	impl.Bootstrapper.hostnameGetter = configReader
	gpUpgradeDir := filepath.Join(os.Getenv("HOME"), ".gp_upgrade")
	impl.Bootstrapper.softwareVerifier = NewClusterSsher(upgradestatus.NewChecklistManager(gpUpgradeDir), logger)
	return impl
}

// CatchAllCliToHubListenerImpl holds many of the behaviors that the hub can do
// which have not yet been implemented in separate purpose-built hub modules
type CatchAllCliToHubListenerImpl struct {
	logger      logger.LogEntry
	clusterPair cluster.PairOperator
}

type CliToHubListenerImpl struct {
	CatchAllCliToHubListenerImpl
	Bootstrapper
}
