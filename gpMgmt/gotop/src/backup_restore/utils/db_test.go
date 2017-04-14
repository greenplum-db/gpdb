package utils_test

import (
	"backup_restore/testutils"
	"backup_restore/utils"
	"os"
	"testing"

	"github.com/jmoiron/sqlx"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"gopkg.in/DATA-DOG/go-sqlmock.v1"
)

var connection *utils.DBConn
var mock sqlmock.Sqlmock

func TestDB(t *testing.T) {
	RegisterFailHandler(Fail)
}

var _ = Describe("utils/db tests", func() {
	BeforeSuite(func() {
		testutils.SetupTestLogger()
	})
	Describe("NewDBConn", func() {
		Context("Database given with -dbname flag", func() {
			It("gets the DBName from dbname argument", func() {
				connection = utils.NewDBConn("testdb")
				Expect(connection.DBName).To(Equal("testdb"))
			})
		})
		Context("No database given with -dbname flag but PGDATABASE set", func() {
			It("gets the DBName from PGDATABASE", func() {
				oldPgDatabase := os.Getenv("PGDATABASE")
				os.Setenv("PGDATABASE", "testdb")
				defer os.Setenv("PGDATABASE", oldPgDatabase)

				connection = utils.NewDBConn("")
				Expect(connection.DBName).To(Equal("testdb"))
			})
		})
		Context("No database given with either -dbname or PGDATABASE", func() {
			It("fails", func() {
				oldPgDatabase := os.Getenv("PGDATABASE")
				os.Setenv("PGDATABASE", "")
				defer os.Setenv("PGDATABASE", oldPgDatabase)

				defer testutils.ShouldPanicWithMessage("No database provided and PGDATABASE not set")
				connection = utils.NewDBConn("")
			})
		})
	})
	Describe("DBConn.Connect", func() {
		Context("The database exists", func() {
			It("connects successfully", func() {
				var mockdb *sqlx.DB
				mockdb, mock = testutils.CreateMockDB()
				driver := utils.TestDriver{DBExists: true, DB: mockdb}
				connection = utils.NewDBConn("testdb")
				connection.Driver = driver
				Expect(connection.DBName).To(Equal("testdb"))
				connection.Connect()
			})
		})
		Context("The database does not exist", func() {
			It("fails", func() {
				var mockdb *sqlx.DB
				mockdb, mock = testutils.CreateMockDB()
				driver := utils.TestDriver{DBExists: false, DB: mockdb, DBName: "testdb"}
				connection = utils.NewDBConn("testdb")
				connection.Driver = driver
				Expect(connection.DBName).To(Equal("testdb"))
				defer testutils.ShouldPanicWithMessage("Database testdb does not exist, exiting")
				connection.Connect()
			})
		})
	})
	Describe("DBConn.Exec", func() {
		It("executes an INSERT outside of a transaction", func() {
			connection, mock = testutils.CreateAndConnectMockDB()
			fakeResult := utils.TestResult{Rows: 1}
			mock.ExpectExec("INSERT (.*)").WillReturnResult(fakeResult)

			res, err := connection.Exec("INSERT INTO pg_tables VALUES ('schema', 'table')")
			Expect(err).ToNot(HaveOccurred())
			rowsReturned, err := res.RowsAffected()
			Expect(rowsReturned).To(Equal(int64(1)))
		})
		It("executes an INSERT in a transaction", func() {
			connection, mock = testutils.CreateAndConnectMockDB()
			fakeResult := utils.TestResult{Rows: 1}
			testutils.ExpectBegin(mock)
			mock.ExpectExec("INSERT (.*)").WillReturnResult(fakeResult)
			mock.ExpectCommit()

			connection.Begin()
			res, err := connection.Exec("INSERT INTO pg_tables VALUES ('schema', 'table')")
			connection.Commit()
			Expect(err).ToNot(HaveOccurred())
			rowsReturned, err := res.RowsAffected()
			Expect(rowsReturned).To(Equal(int64(1)))
		})
	})
	Describe("DBConn.Get", func() {
		It("executes a GET outside of a transaction", func() {
			connection, mock = testutils.CreateAndConnectMockDB()
			two_col_single_row := sqlmock.NewRows([]string{"schemaname", "tablename"}).
				AddRow("schema1", "table1")
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(two_col_single_row)

			testRecord := struct {
				Schemaname string
				Tablename  string
			}{}

			err := connection.Get(&testRecord, "SELECT schemaname, tablename FROM two_columns ORDER BY schemaname")

			Expect(err).ToNot(HaveOccurred())
			Expect(testRecord.Schemaname).To(Equal("schema1"))
			Expect(testRecord.Tablename).To(Equal("table1"))
		})
		It("executes a GET in a transaction", func() {
			connection, mock = testutils.CreateAndConnectMockDB()
			two_col_single_row := sqlmock.NewRows([]string{"schemaname", "tablename"}).
				AddRow("schema1", "table1")
			testutils.ExpectBegin(mock)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(two_col_single_row)
			mock.ExpectCommit()

			testRecord := struct {
				Schemaname string
				Tablename  string
			}{}

			connection.Begin()
			err := connection.Get(&testRecord, "SELECT schemaname, tablename FROM two_columns ORDER BY schemaname")
			connection.Commit()
			Expect(err).ToNot(HaveOccurred())
			Expect(testRecord.Schemaname).To(Equal("schema1"))
			Expect(testRecord.Tablename).To(Equal("table1"))
		})
	})
	Describe("DBConn.Select", func() {
		It("executes a SELECT outside of a transaction", func() {
			connection, mock = testutils.CreateAndConnectMockDB()
			two_col_rows := sqlmock.NewRows([]string{"schemaname", "tablename"}).
				AddRow("schema1", "table1").
				AddRow("schema2", "table2")
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(two_col_rows)

			testSlice := make([]struct {
				Schemaname string
				Tablename  string
			}, 0)

			err := connection.Select(&testSlice, "SELECT schemaname, tablename FROM two_columns ORDER BY schemaname LIMIT 2")

			Expect(err).ToNot(HaveOccurred())
			Expect(len(testSlice)).To(Equal(2))
			Expect(testSlice[0].Schemaname).To(Equal("schema1"))
			Expect(testSlice[0].Tablename).To(Equal("table1"))
			Expect(testSlice[1].Schemaname).To(Equal("schema2"))
			Expect(testSlice[1].Tablename).To(Equal("table2"))
		})
		It("executes a SELECT in a transaction", func() {
			connection, mock = testutils.CreateAndConnectMockDB()
			two_col_rows := sqlmock.NewRows([]string{"schemaname", "tablename"}).
				AddRow("schema1", "table1").
				AddRow("schema2", "table2")
			testutils.ExpectBegin(mock)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(two_col_rows)
			mock.ExpectCommit()

			testSlice := make([]struct {
				Schemaname string
				Tablename  string
			}, 0)

			connection.Begin()
			err := connection.Select(&testSlice, "SELECT schemaname, tablename FROM two_columns ORDER BY schemaname LIMIT 2")
			connection.Commit()

			Expect(err).ToNot(HaveOccurred())
			Expect(len(testSlice)).To(Equal(2))
			Expect(testSlice[0].Schemaname).To(Equal("schema1"))
			Expect(testSlice[0].Tablename).To(Equal("table1"))
			Expect(testSlice[1].Schemaname).To(Equal("schema2"))
			Expect(testSlice[1].Tablename).To(Equal("table2"))
		})
	})
	Describe("GetUniqueSchemas", func() {
		publicOne := utils.Table{0, "public", "foo"}
		publicTwo := utils.Table{0, "public", "bar"}
		otherOne := utils.Table{0, "otherschema", "foo"}
		otherTwo := utils.Table{0, "otherschema", "bar"}

		It("has multiple tables with the public schema", func() {
			tables := []utils.Table{publicOne, publicTwo}
			schemas := utils.GetUniqueSchemas(tables)
			Expect(schemas).To(Equal([]string{}))
		})
		It("has multiple tables with a non-public schema", func() {
			tables := []utils.Table{otherOne, otherTwo}
			schemas := utils.GetUniqueSchemas(tables)
			Expect(schemas).To(Equal([]string{"otherschema"}))
		})
		It("has no tables", func() {
			tables := []utils.Table{}
			schemas := utils.GetUniqueSchemas(tables)
			Expect(schemas).To(Equal([]string{}))
		})
		It("has multiple schemas, each with multiple tables", func() {
			tables := []utils.Table{publicOne, publicTwo, otherOne, otherTwo}
			schemas := utils.GetUniqueSchemas(tables)
			Expect(schemas).To(Equal([]string{"otherschema"}))
		})
	})
})
