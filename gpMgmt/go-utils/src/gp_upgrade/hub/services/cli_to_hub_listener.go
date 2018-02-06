package services

import (
	"gp_upgrade/hub/cluster"
	"gp_upgrade/hub/configutils"
	"gp_upgrade/hub/logger"
)

func NewCliToHubListener(logger logger.LogEntry, pair cluster.PairOperator) *CliToHubListenerImpl {
	impl := &CliToHubListenerImpl{}
	impl.CatchAllCliToHubListenerImpl.logger = logger
	impl.CatchAllCliToHubListenerImpl.clusterPair = pair
	configReader := configutils.NewReader()
	configReader.OfOldClusterConfig() // refactor opportunity -- don't use this pattern, use different types or separate functions for old/new or set the config path at reader initialization time
	impl.Bootstrapper.hostnameGetter = configReader
	impl.Bootstrapper.softwareVerifier = NewClusterSsher(NewChecklistWriterImpl())
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
