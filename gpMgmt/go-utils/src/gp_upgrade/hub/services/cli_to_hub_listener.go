package services

import (
	"gp_upgrade/hub/cluster"
	"gp_upgrade/hub/logger"
)

func NewCliToHubListener(logger logger.LogEntry, pair cluster.PairOperator) *CliToHubListenerImpl {
	impl := &CliToHubListenerImpl{}
	impl.CatchAllCliToHubListenerImpl.logger = logger
	impl.CatchAllCliToHubListenerImpl.clusterPair = pair
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
