package test_utils

import (
	"fmt"
	"gp_upgrade/config"
	"gp_upgrade/db"
	"gpbackup/testutils"
	"io/ioutil"
	"os"

	"github.com/jmoiron/sqlx"
	. "github.com/onsi/ginkgo"
	"gopkg.in/DATA-DOG/go-sqlmock.v1"
)

const (
	TempHomeDir = "/tmp/gp_upgrade_test_temp_home_dir"

	SAMPLE_JSON = `[{
    "address": "briarwood",
    "content": 2,
    "dbid": 7,
    "fsedbid": 7,
    "fsefsoid": "3052",
    "fselocation": "/Users/pivotal/workspace/gpdb/gpAux/gpdemo/datadirs/dbfast_mirror3/demoDataDir2",
    "hostname": "briarwood",
    "mode": "s",
    "port": 25437,
    "preferred_role": "m",
    "replication_port": 25443,
    "role": "m",
    "san_mounts": null,
    "status": "u"
  }]`
)

func Check(msg string, e error) {
	if e != nil {
		panic(fmt.Sprintf("%s: %s\n", msg, e.Error()))
	}
}

func SetHomeDir(temp_home_dir string) string {
	save := os.Getenv("HOME")
	err := os.MkdirAll(temp_home_dir, 0700)
	Check("cannot create home temp dir", err)
	err = os.Setenv("HOME", temp_home_dir)
	Check("cannot set home dir", err)
	return save
}

func ResetTempHomeDir() string {
	err := os.RemoveAll(TempHomeDir)
	Check("cannot remove temp home", err)
	return SetHomeDir(TempHomeDir)
}

func WriteSampleConfig() {
	err := os.MkdirAll(config.GetConfigDir(), 0700)
	Check("cannot create sample dir", err)
	err = ioutil.WriteFile(config.GetConfigFilePath(), []byte(SAMPLE_JSON), 0600)
	Check("cannot write sample config", err)
}

func createMockDB() (*sqlx.DB, sqlmock.Sqlmock) {
	db, mock, err := sqlmock.New()
	mockdb := sqlx.NewDb(db, "sqlmock")
	if err != nil {
		Fail("Could not create mock database connection")
	}
	return mockdb, mock
}

func CreateMockDBConn(masterHost string, masterPort int) (*db.DBConn, sqlmock.Sqlmock) {
	mockdb, mock := createMockDB()
	dbConn := db.NewDBConn(masterHost, masterPort, "testdb", "", "")
	dbConn.Driver = testutils.TestDriver{DBExists: true, DB: mockdb, DBName: "testdb"}
	if dbConn.Conn != nil && dbConn.Conn.Stats().OpenConnections > 0 {
		Fail("connection before connect is called")
	}
	return dbConn, mock
}
