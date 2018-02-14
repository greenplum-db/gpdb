package services

import (
	pb "gp_upgrade/idl"

	"golang.org/x/net/context"

	"errors"
	"fmt"
	"gp_upgrade/utils"
	"path"
)

func (s *CatchAllCliToHubListenerImpl) PrepareShutdownClusters(ctx context.Context,
	in *pb.PrepareShutdownClustersRequest) (*pb.PrepareShutdownClustersReply, error) {
	s.logger.Info <- fmt.Sprintf("starting PrepareShutdownClusters()")

	// will be initialized for future uses also? We think so -- it should
	initErr := s.clusterPair.Init(in.OldBinDir, in.NewBinDir)
	if initErr != nil {
		s.logger.Error <- fmt.Sprintf("An occurred during cluster pair init: %v", initErr)
		return nil, initErr
	}

	homeDirectory := utils.System.Getenv("HOME")
	if homeDirectory == "" {
		return nil, errors.New("Could not find the home directory environment variable")

	}
	pathToGpstopStateDir := path.Join(homeDirectory, ".gp_upgrade", "gpstop")
	go s.clusterPair.StopEverything(pathToGpstopStateDir, &s.logger)

	/* TODO: gpstop may take a while.
	 * How do we check if everything is stopped?
	 * Should we check bindirs for 'good-ness'? No...

	 * Use go routine along with using files as a way to keep track of gpstop state
	 */

	// XXX: May be tell user to run status, or if that seems stuck, check gpAdminLogs/gp_upgrade_hub*.log

	return &pb.PrepareShutdownClustersReply{}, nil
}
