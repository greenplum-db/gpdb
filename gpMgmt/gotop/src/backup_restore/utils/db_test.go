package utils_test

import (
	"backup_restore/utils"
	"database/sql"
	"fmt"
	"os"
	"strings"
	"testing"

	"github.com/jmoiron/sqlx"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"gopkg.in/DATA-DOG/go-sqlmock"
)

var connection *utils.DBConn
var mock sqlmock.Sqlmock

func TestDB(t *testing.T) {
	RegisterFailHandler(Fail)
	RunSpecs(t, "db.go unit tests")
}

func createMockDB() *sqlx.DB {
	var db *sql.DB
	var err error
	db, mock, err = sqlmock.New()
	mockdb := sqlx.NewDb(db, "sqlmock")
	if err != nil {
		Fail("Could not create mock database connection")
	}
	return mockdb
}

func expectBegin() {
	fakeResult := utils.TestResult{Rows: 0}
	mock.ExpectBegin()
	mock.ExpectExec("SET TRANSACTION(.*)").WillReturnResult(fakeResult)
}

func createAndConnectMockDB() {
	driver := utils.TestDriver{DBExists: true, DB: createMockDB(), DBName: "testdb"}
	connection = utils.NewDBConn("testdb")
	connection.Driver = driver
	connection.Connect()
}

func shouldPanicWithMessage(message string) {
	if r := recover(); r != nil {
		errMsg := strings.TrimSpace(fmt.Sprintf("%v", r))
		if errMsg != message {
			Fail(fmt.Sprintf("Expected panic message '%s', got '%s'", message, errMsg))
		}
	} else {
		Fail("Function did not panic as expected")
	}
}

var _ = Describe("utils/db tests", func() {
	Describe("NewDBConn", func() {
		Context("Database given with -dbname flag", func() {
			It("Should get the DBName from dbname argument", func() {
				connection = utils.NewDBConn("testdb")
				Expect(connection.DBName).To(Equal("testdb"))
			})
		})
		Context("No database given with -dbname flag but PGDATABASE set", func() {
			It("Should get the DBName from PGDATABASE", func() {
				oldPgDatabase := os.Getenv("PGDATABASE")
				os.Setenv("PGDATABASE", "testdb")
				defer os.Setenv("PGDATABASE", oldPgDatabase)

				connection = utils.NewDBConn("")
				Expect(connection.DBName).To(Equal("testdb"))
			})
		})
		Context("No database given with either -dbname or PGDATABASE", func() {
			It("Should fail", func() {
				oldPgDatabase := os.Getenv("PGDATABASE")
				os.Setenv("PGDATABASE", "")
				defer os.Setenv("PGDATABASE", oldPgDatabase)

				defer shouldPanicWithMessage("No database provided and PGDATABASE not set")
				connection = utils.NewDBConn("")
			})
		})
	})
	Describe("DBConn.Connect", func() {
		Context("The database exists", func() {
			It("Should connect successfully", func() {
				driver := utils.TestDriver{DBExists: true, DB: createMockDB()}
				connection = utils.NewDBConn("testdb")
				connection.Driver = driver
				Expect(connection.DBName).To(Equal("testdb"))
				connection.Connect()
			})
		})
		Context("The database does not exist", func() {
			It("Should fail", func() {
				driver := utils.TestDriver{DBExists: false, DB: createMockDB(), DBName: "testdb"}
				connection = utils.NewDBConn("testdb")
				connection.Driver = driver
				Expect(connection.DBName).To(Equal("testdb"))
				defer shouldPanicWithMessage("Database testdb does not exist, exiting")
				connection.Connect()
			})
		})
	})
	Describe("DBConn.Exec", func() {
		It("Should be able to INSERT outside of a transaction", func() {
			createAndConnectMockDB()
			fakeResult := utils.TestResult{Rows: 1}
			mock.ExpectExec("INSERT (.*)").WillReturnResult(fakeResult)

			res, err := connection.Exec("INSERT INTO pg_tables VALUES ('schema', 'table')")
			Expect(err).ToNot(HaveOccurred())
			rowsReturned, err := res.RowsAffected()
			Expect(rowsReturned).To(Equal(int64(1)))
		})
		It("Should be able to INSERT in a transaction", func() {
			createAndConnectMockDB()
			fakeResult := utils.TestResult{Rows: 1}
			expectBegin()
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
		It("Should be able to GET outside of a transaction", func() {
			createAndConnectMockDB()
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
		It("Should be able to GET in a transaction", func() {
			createAndConnectMockDB()
			two_col_single_row := sqlmock.NewRows([]string{"schemaname", "tablename"}).
				AddRow("schema1", "table1")
			expectBegin()
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
		It("Should be able to SELECT outside of a transaction", func() {
			createAndConnectMockDB()
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
		It("Should be able to SELECT in a transaction", func() {
			createAndConnectMockDB()
			two_col_rows := sqlmock.NewRows([]string{"schemaname", "tablename"}).
				AddRow("schema1", "table1").
				AddRow("schema2", "table2")
			expectBegin()
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
})
