package db_test

import (
	"gp_upgrade/db"
	"gpbackup/testutils"
	"os"

	"gp_upgrade/test_utils"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var connection *db.DBConn

var _ = Describe("db tests", func() {
	Describe("NewDBConn", func() {
		Context("Database connection receives its constructor parameters", func() {
			It("gets the DBName from dbname argument, port from masterPort, and host from masterHost", func() {
				connection = db.NewDBConn("localhost", 5432, "testdb")
				Expect(connection.DBName).To(Equal("testdb"))
				Expect(connection.Host).To(Equal("localhost"))
				Expect(connection.Port).To(Equal(5432))
			})
		})
		Context("No database given with -dbname flag but PGDATABASE set", func() {
			It("gets the DBName from PGDATABASE", func() {
				oldPgDatabase := os.Getenv("PGDATABASE")
				os.Setenv("PGDATABASE", "testdb")
				defer os.Setenv("PGDATABASE", oldPgDatabase)

				connection = db.NewDBConn("localhost", 5432, "testdb")
				Expect(connection.DBName).To(Equal("testdb"))
			})
		})
		Context("No database given with either -dbname or PGDATABASE", func() {
			It("fails", func() {
				oldPgDatabase := os.Getenv("PGDATABASE")
				os.Setenv("PGDATABASE", "")
				defer os.Setenv("PGDATABASE", oldPgDatabase)

				connection = db.NewDBConn("localhost", 5432, "")
				err := connection.Connect()
				Expect(err).To(HaveOccurred())
			})
		})
	})
	Describe("DBConn.Connect", func() {
		Context("The database exists", func() {
			It("connects successfully", func() {
				var mockDBConn *db.DBConn
				mockDBConn, _ = test_utils.CreateMockDBConn("localhost", 5432)
				connection = db.NewDBConn("localhost", 5432, "testdb")
				connection.Driver = testutils.TestDriver{DBExists: true, DB: mockDBConn.Conn}
				err := connection.Connect()
				defer connection.Close()
				Expect(err).ToNot(HaveOccurred())
			})
		})
		Context("The database does not exist", func() {
			It("fails", func() {
				var mockDBConn *db.DBConn
				mockDBConn, _ = test_utils.CreateMockDBConn("localhost", 5432)
				connection = db.NewDBConn("localhost", 5432, "testdb")
				connection.Driver = testutils.TestDriver{DBExists: false, DB: mockDBConn.Conn}
				err := connection.Connect()
				Expect(err).To(HaveOccurred())
			})
		})
	})
})
