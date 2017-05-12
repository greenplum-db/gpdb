package backup_test

import (
	"backup_restore/backup"
	"backup_restore/testutils"
	"backup_restore/utils"
	"testing"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"gopkg.in/DATA-DOG/go-sqlmock.v1"
)

func TestData(t *testing.T) {
	RegisterFailHandler(Fail)
}

var _ = Describe("backup/data tests", func() {
	var connection *utils.DBConn
	var mock sqlmock.Sqlmock
	BeforeEach(func() {
		connection, mock = testutils.CreateAndConnectMockDB()
		testutils.SetupTestLogger()
	})
	Describe("CopyTableOut", func() {
		It("will dump a table to its own file", func() {
			testTable := utils.Table{2345, 3456, "public", "foo"}
			utils.DumpPathFmtStr = "<SEG_DATA_DIR>/backups/20170101/20170101010101"
			execStr := "COPY public.foo TO '<SEG_DATA_DIR>/backups/20170101/20170101010101/gpbackup_<SEGID>_20170101010101_3456' WITH CSV DELIMITER ',' ON SEGMENT;"
			utils.DumpTimestamp = "20170101010101"
			mock.ExpectExec(execStr).WillReturnResult(sqlmock.NewResult(10, 0))
			backup.CopyTableOut(connection, testTable)
		})
	})
})
