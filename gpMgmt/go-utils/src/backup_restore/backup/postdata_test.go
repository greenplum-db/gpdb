package backup_test

import (
	"backup_restore/backup"
	"backup_restore/testutils"
	"backup_restore/utils"
	"database/sql/driver"
	"testing"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"gopkg.in/DATA-DOG/go-sqlmock.v1"
)

func TestPostdata(t *testing.T) {
	RegisterFailHandler(Fail)
}

var _ = Describe("backup/postdata tests", func() {
	var connection *utils.DBConn
	var mock sqlmock.Sqlmock
	BeforeEach(func() {
		connection, mock = testutils.CreateAndConnectMockDB()
		testutils.SetupTestLogger()
	})

	Describe("GetIndexesForAllTables", func() {
		tableOne := utils.Table{0, 0, "public", "table_one"}
		tableTwo := utils.Table{0, 0, "public", "table_two"}
		tableWithout := utils.Table{0, 0, "public", "table_no_index"}

		header := []string{"indexdef"}
		resultEmpty := sqlmock.NewRows(header)

		Context("Indexes on a single column", func() {
			It("returns a slice containing one CREATE INDEX statement for one table", func() {
				testTables := []utils.Table{tableOne}
				rowOneIndex := []driver.Value{"CREATE INDEX btree_idx1 ON table_one USING btree (i)"}
				resultOne := sqlmock.NewRows(header).AddRow(rowOneIndex...)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(resultOne)
				indexes := backup.GetIndexesForAllTables(connection, testTables)
				Expect(len(indexes)).To(Equal(1))
				Expect(indexes[0]).To(Equal("\n\nCREATE INDEX btree_idx1 ON table_one USING btree (i);"))
			})
			It("returns a slice containing one CREATE INDEX statement for two tables", func() {
				testTables := []utils.Table{tableOne, tableTwo}
				rowOneIndex := []driver.Value{"CREATE INDEX btree_idx1 ON table_one USING btree (i)"}
				rowTwoIndex := []driver.Value{"CREATE INDEX btree_idx2 ON table_two USING btree (j)"}
				resultOne := sqlmock.NewRows(header).AddRow(rowOneIndex...)
				resultTwo := sqlmock.NewRows(header).AddRow(rowTwoIndex...)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(resultOne)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(resultTwo)
				indexes := backup.GetIndexesForAllTables(connection, testTables)
				Expect(len(indexes)).To(Equal(2))
				Expect(indexes[0]).To(Equal("\n\nCREATE INDEX btree_idx1 ON table_one USING btree (i);"))
				Expect(indexes[1]).To(Equal("\n\nCREATE INDEX btree_idx2 ON table_two USING btree (j);"))
			})
			It("returns a slice containing two CREATE INDEX statement for one table", func() {
				testTables := []utils.Table{tableOne, tableTwo}
				rowOneIndexOne := []driver.Value{"CREATE INDEX btree_idx1 ON table_one USING btree (i)"}
				rowOneIndexTwo := []driver.Value{"CREATE INDEX bitmap_idx1 ON table_one USING bitmap (i)"}
				rowTwoIndex := []driver.Value{"CREATE INDEX btree_idx2 ON table_two USING btree (j)"}
				resultOne := sqlmock.NewRows(header).AddRow(rowOneIndexOne...).AddRow(rowOneIndexTwo...)
				resultTwo := sqlmock.NewRows(header).AddRow(rowTwoIndex...)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(resultOne)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(resultTwo)
				indexes := backup.GetIndexesForAllTables(connection, testTables)
				Expect(len(indexes)).To(Equal(3))
				Expect(indexes[0]).To(Equal("\n\nCREATE INDEX btree_idx1 ON table_one USING btree (i);"))
				Expect(indexes[1]).To(Equal("\n\nCREATE INDEX bitmap_idx1 ON table_one USING bitmap (i);"))
				Expect(indexes[2]).To(Equal("\n\nCREATE INDEX btree_idx2 ON table_two USING btree (j);"))
			})
			It("returns a slice containing one CREATE INDEX statement when one table has an index and one does not", func() {
				testTables := []utils.Table{tableOne, tableWithout}
				rowOneIndex := []driver.Value{"CREATE INDEX btree_idx1 ON table_one USING btree (i)"}
				resultOne := sqlmock.NewRows(header).AddRow(rowOneIndex...)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(resultOne)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(resultEmpty)
				indexes := backup.GetIndexesForAllTables(connection, testTables)
				Expect(len(indexes)).To(Equal(1))
				Expect(indexes[0]).To(Equal("\n\nCREATE INDEX btree_idx1 ON table_one USING btree (i);"))
			})
		})
	})
})
