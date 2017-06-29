package db_test

import (
	"github.com/greenplum-db/gpbackup/testutils"

	"gp_upgrade/test_utils"

	"os"

	"gp_upgrade/db"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("db tests", func() {
	Describe("NewDBConn", func() {
		Context("Database connection receives its constructor parameters", func() {
			It("gets the DBName from dbname argument, port from masterPort, and host from masterHost", func() {
				dbConnector := db.NewDBConn("localhost", 5432, "testdb", "", "")
				connectorStruct := dbConnector.(*db.GPDBConnector)
				Expect(connectorStruct.DBName).To(Equal("testdb"))
				Expect(connectorStruct.Host).To(Equal("localhost"))
				Expect(connectorStruct.Port).To(Equal(5432))
			})
		})
		Context("No database given with -dbname flag but PGDATABASE set", func() {
			It("gets the DBName from PGDATABASE", func() {
				oldPgDatabase := os.Getenv("PGDATABASE")
				os.Setenv("PGDATABASE", "testdb")
				defer os.Setenv("PGDATABASE", oldPgDatabase)

				dbConnector := db.NewDBConn("localhost", 5432, "testdb", "", "")
				connectorStruct := dbConnector.(*db.GPDBConnector)
				Expect(connectorStruct.DBName).To(Equal("testdb"))
			})
		})
		Context("No database given with either -dbname or PGDATABASE", func() {
			It("fails", func() {
				oldPgDatabase := os.Getenv("PGDATABASE")
				os.Setenv("PGDATABASE", "")
				defer os.Setenv("PGDATABASE", oldPgDatabase)

				connection := db.NewDBConn("localhost", 5432, "", "", "")
				err := connection.Connect()
				Expect(err).To(HaveOccurred())
			})
		})
	})
	Describe("GPDBConnector.Connect", func() {
		Context("The database exists", func() {
			It("connects successfully", func() {
				var mockDBConn *db.GPDBConnector
				mockDBConn, _ = test_utils.CreateMockDBConn("localhost", 5432)
				err := mockDBConn.Connect()
				defer mockDBConn.Close()
				Expect(err).ToNot(HaveOccurred())
			})
		})
		Context("The database does not exist", func() {
			It("fails", func() {
				mockdb, _ := test_utils.CreateMockDB()
				gpdbConnector := &db.GPDBConnector{
					Driver: testutils.TestDriver{DBExists: false, DB: mockdb, DBName: "testdb"},
					DBName: "testdb",
					Host:   "localhost",
					Port:   5432,
				}
				err := gpdbConnector.Connect()
				Expect(err).To(HaveOccurred())
			})
		})
	})
})
