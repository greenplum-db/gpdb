package services

import (
	"gp_upgrade/db"
	pb "gp_upgrade/idl"
	"gp_upgrade/utils"

	gpbackupUtils "github.com/greenplum-db/gp-common-go-libs/gplog"
	"github.com/jmoiron/sqlx"
	"github.com/pkg/errors"
	"golang.org/x/net/context"
)

func (s *CatchAllCliToHubListenerImpl) CheckObjectCount(ctx context.Context,
	in *pb.CheckObjectCountRequest) (*pb.CheckObjectCountReply, error) {

	gpbackupUtils.Info("starting CheckObjectCount")

	dbConnector := db.NewDBConn("localhost", int(in.DbPort), "template1")
	defer dbConnector.Close()
	err := dbConnector.Connect()
	if err != nil {
		gpbackupUtils.Error(err.Error())
		return nil, utils.DatabaseConnectionError{Parent: err}
	}
	databaseHandler := dbConnector.GetConn()
	names, err := GetDbList(databaseHandler)
	if err != nil {
		gpbackupUtils.Error(err.Error())
		return nil, errors.New(err.Error())
	}

	var results []*pb.CountPerDb
	for i := 0; i < len(names); i++ {

		dbConnector = db.NewDBConn("localhost", int(in.DbPort), names[i])
		defer dbConnector.Close()
		err = dbConnector.Connect()
		if err != nil {
			gpbackupUtils.Error(err.Error())
			return nil, errors.New(err.Error())
		}
		databaseHandler = dbConnector.GetConn()
		aocount, heapcount, errFromCounts := GetCountsForDb(databaseHandler)
		if errFromCounts != nil {
			gpbackupUtils.Error(err.Error())
			return nil, errors.New(errFromCounts.Error())
		}
		results = append(results, &pb.CountPerDb{DbName: names[i], AoCount: aocount, HeapCount: heapcount})
	}

	successReply := &pb.CheckObjectCountReply{ListOfCounts: results}
	return successReply, nil
}

func GetDbList(dbHandler *sqlx.DB) ([]string, error) {

	dbNames := []string{}
	err := dbHandler.Select(&dbNames, GET_DATABASE_NAMES)
	if err != nil {
		gpbackupUtils.Error(err.Error())
		return nil, errors.New(err.Error())
	}

	return dbNames, nil
}

func GetCountsForDb(dbHandler *sqlx.DB) (int32, int32, error) {

	var aoCount, heapCount int32

	err := dbHandler.Get(&aoCount, AO_CO_TABLE_QUERY_COUNT)
	if err != nil {
		gpbackupUtils.Error(err.Error())
		return aoCount, heapCount, errors.New(err.Error())
	}

	err = dbHandler.Get(&heapCount, HEAP_TABLE_QUERY_COUNT)
	if err != nil {
		gpbackupUtils.Error(err.Error())
		return aoCount, heapCount, errors.New(err.Error())
	}

	return aoCount, heapCount, nil
}

const (
	GET_DATABASE_NAMES = `SELECT datname FROM pg_database WHERE datname != 'template0'`
	/* "::" casting is specific to Postgres.
	 * changed sql to an ANSI standard casting
		-- COUNT THE NUMBER OF APPEND ONLY OBJECTS ON THE SYSTEM
	*/
	AO_CO_TABLE_QUERY_COUNT = `
	SELECT COUNT(*)
	  FROM pg_class c
	  JOIN pg_namespace n ON c.relnamespace = n.oid
	WHERE c.relkind = cast('r' as CHAR)                       -- All tables (including partitions)
	  AND c.relstorage IN ('a','c')                           -- AO / CO
	  AND n.nspname NOT LIKE 'pg_temp_%'                      -- not temp tables
	  AND c.oid >= 16384                                      -- No system tables
	  AND (c.relnamespace >= 16384 OR n.nspname = 'public')   -- No system schemas, but include 'public'
	  AND (NOT relhassubclass                                 -- not partition parent tables
	       OR ( relhassubclass
		    AND NOT EXISTS ( SELECT oid FROM pg_partition_rule p WHERE c.oid = p.parchildrelid )
		    AND NOT EXISTS ( SELECT oid FROM pg_partition p WHERE c.oid = p.parrelid )
		)
	);
	`

	HEAP_TABLE_QUERY_COUNT = `
	SELECT COUNT(*)
	  FROM pg_class c
	  JOIN pg_namespace n ON c.relnamespace = n.oid
	WHERE c.relkind = cast('r' as CHAR)                       -- All tables (including partitions)
	  AND c.relstorage NOT IN ('a','c')                       -- NON AO / CO
	  AND n.nspname NOT LIKE 'pg_temp_%'                      -- not temp tables
	  AND c.oid >= 16384                                      -- No system tables
	  AND (c.relnamespace >= 16384 OR n.nspname = 'public')   -- No system schemas, but include 'public'
	  AND (NOT relhassubclass                                 -- not partition parent tables
	       OR ( relhassubclass
		    AND NOT EXISTS ( SELECT oid FROM pg_partition_rule p WHERE c.oid = p.parchildrelid )
		    AND NOT EXISTS ( SELECT oid FROM pg_partition p WHERE c.oid = p.parrelid )
		)
	  );
	`
)
