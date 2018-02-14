package services

import (
	"gp_upgrade/db"
	"gp_upgrade/hub/configutils"
	pb "gp_upgrade/idl"
	"gp_upgrade/utils"

	gpbackupUtils "github.com/greenplum-db/gp-common-go-libs/gplog"
	"github.com/jmoiron/sqlx"
	"github.com/pkg/errors"
	"golang.org/x/net/context"
)

func (s *CatchAllCliToHubListenerImpl) CheckConfig(ctx context.Context,
	in *pb.CheckConfigRequest) (*pb.CheckConfigReply, error) {

	gpbackupUtils.Info("starting CheckConfig()")
	dbConnector := db.NewDBConn("localhost", int(in.DbPort), "template1")
	defer dbConnector.Close()
	err := dbConnector.Connect()
	if err != nil {
		gpbackupUtils.Error(err.Error())
		return nil, utils.DatabaseConnectionError{Parent: err}
	}
	databaseHandler := dbConnector.GetConn()

	configQuery := `select dbid, content, role, preferred_role,
		mode, status, port, hostname, address, datadir
		from gp_segment_configuration`
	err = SaveQueryResultToJSON(databaseHandler, configQuery,
		configutils.NewWriter(configutils.GetConfigFilePath()))
	if err != nil {
		gpbackupUtils.Error(err.Error())
		return nil, err
	}

	versionQuery := `show gp_server_version_num`
	err = SaveQueryResultToJSON(databaseHandler, versionQuery,
		configutils.NewWriter(configutils.GetVersionFilePath()))
	if err != nil {
		gpbackupUtils.Error(err.Error())
		return nil, err
	}

	successReply := &pb.CheckConfigReply{ConfigStatus: "All good"}
	return successReply, nil
}

// public for testing purposes
func SaveQueryResultToJSON(databaseHandler *sqlx.DB, configQuery string, writer configutils.Store) error {
	rows, err := databaseHandler.Query(configQuery)
	if err != nil {
		gpbackupUtils.Error(err.Error())
		return errors.New(err.Error())
	}
	defer rows.Close()

	err = writer.Load(rows)
	if err != nil {
		gpbackupUtils.Error(err.Error())
		return errors.New(err.Error())
	}

	err = writer.Write()
	if err != nil {
		gpbackupUtils.Error(err.Error())
		return errors.New(err.Error())
	}

	return nil
}
