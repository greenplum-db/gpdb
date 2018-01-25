package services

import (
	"errors"
	"gp_upgrade/db"
	pb "gp_upgrade/idl"
	"gp_upgrade/utils"
	"regexp"

	"github.com/cppforlife/go-semi-semantic/version"
	gpbackupUtils "github.com/greenplum-db/gpbackup/utils"
	"github.com/jmoiron/sqlx"
	"golang.org/x/net/context"
)

func (s *CatchAllCliToHubListenerImpl) CheckVersion(ctx context.Context,
	in *pb.CheckVersionRequest) (*pb.CheckVersionReply, error) {

	gpbackupUtils.GetLogger().Info("starting CheckVersion")
	dbConnector := db.NewDBConn(in.Host, int(in.DbPort), "template1")
	defer dbConnector.Close()
	err := dbConnector.Connect()
	if err != nil {
		gpbackupUtils.GetLogger().Error(err.Error())
		return nil, utils.DatabaseConnectionError{Parent: err}
	}
	databaseHandler := dbConnector.GetConn()
	isVersionCompatible, err := VerifyVersion(databaseHandler)
	if err != nil {
		gpbackupUtils.GetLogger().Error(err.Error())
		return nil, errors.New(err.Error())
	}
	return &pb.CheckVersionReply{IsVersionCompatible: isVersionCompatible}, nil
}

func VerifyVersion(dbHandler *sqlx.DB) (bool, error) {
	var row string
	err := dbHandler.Get(&row, VERSION_QUERY)
	if err != nil {
		gpbackupUtils.GetLogger().Error(err.Error())
		return false, errors.New(err.Error())
	}
	re := regexp.MustCompile("Greenplum Database (.*) build")
	versionStringResults := re.FindStringSubmatch(row)
	if len(versionStringResults) < 2 {
		gpbackupUtils.GetLogger().Error("Didn't get a version string match")
		return false, errors.New("Didn't get a version string match")
	}
	versionString := versionStringResults[1]
	versionObject, err := version.NewVersionFromString(versionString)
	if err != nil {
		gpbackupUtils.GetLogger().Error(err.Error())
		return false, err
	}
	if versionObject.IsGt(version.MustNewVersionFromString(MINIMUM_VERSION)) {
		return true, nil
	}
	gpbackupUtils.GetLogger().Error("falling through")
	return false, nil
}

const (
	VERSION_QUERY   = `SELECT version()`
	MINIMUM_VERSION = "4.3.9.0"
)
