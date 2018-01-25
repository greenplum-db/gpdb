package services

import (
	"gp_upgrade/db"
	"gp_upgrade/hub/configutils"
	pb "gp_upgrade/idl"
	"gp_upgrade/utils"

	gpbackupUtils "github.com/greenplum-db/gpbackup/utils"
	"golang.org/x/net/context"
)

func (s *CatchAllCliToHubListenerImpl) PrepareInitCluster(ctx context.Context,
	in *pb.PrepareInitClusterRequest) (*pb.PrepareInitClusterReply, error) {

	gpbackupUtils.GetLogger().Info("starting PrepareInitCluster()")
	dbConnector := db.NewDBConn("localhost", int(in.DbPort), "template1")
	defer dbConnector.Close()
	err := dbConnector.Connect()
	if err != nil {
		gpbackupUtils.GetLogger().Error(err.Error())
		return nil, utils.DatabaseConnectionError{Parent: err}
	}
	databaseHandler := dbConnector.GetConn()

	configQuery := `select dbid, content, role, preferred_role,
		mode, status, port, hostname, address, datadir
		from gp_segment_configuration`
	err = SaveQueryResultToJSON(databaseHandler, configQuery,
		configutils.NewWriter(configutils.GetNewClusterConfigFilePath()))
	if err != nil {
		gpbackupUtils.GetLogger().Error(err.Error())
		return nil, err
	}
	return &pb.PrepareInitClusterReply{}, nil
}
