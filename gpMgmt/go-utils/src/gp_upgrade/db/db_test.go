package db

import (
	"github.com/greenplum-db/gpbackup/testutils"

	"os"

	"github.com/greenplum-db/gpbackup/utils"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

var _ = Describe("db connector", func() {
	Describe("NewDBConn", func() {
		Context("Database connection receives its constructor parameters", func() {
			It("gets the dbName from dbname argument, port from masterPort, and host from masterHost", func() {
				dbConnector := NewDBConn("localhost", 5432, "testdb")
				connectorStruct := dbConnector.(*GPDBConnector)
				Expect(connectorStruct.dbName).To(Equal("testdb"))
				Expect(connectorStruct.host).To(Equal("localhost"))
				Expect(connectorStruct.port).To(Equal(5432))
			})
		})
		Context("when dbname param is empty, but PGDATABASE env var set", func() {
			It("gets the dbName from PGDATABASE", func() {
				oldPgDatabase := os.Getenv("PGDATABASE")
				os.Setenv("PGDATABASE", "testdb")
				defer os.Setenv("PGDATABASE", oldPgDatabase)

				dbConnector := NewDBConn("localhost", 5432, "testdb")
				connectorStruct := dbConnector.(*GPDBConnector)
				Expect(connectorStruct.dbName).To(Equal("testdb"))
			})
		})
		Context("when dbname param empty and PGDATABASE env var empty", func() {
			It("has an empty database name", func() {
				oldPgDatabase := os.Getenv("PGDATABASE")
				os.Setenv("PGDATABASE", "")
				defer os.Setenv("PGDATABASE", oldPgDatabase)

				dbConnector := NewDBConn("localhost", 5432, "")
				connectorStruct := dbConnector.(*GPDBConnector)
				Expect(connectorStruct.dbName).To(Equal(""))
			})
		})
		Context("when host parameter is empty, but PGHOST is set", func() {
			It("uses PGHOST value", func() {
				old := os.Getenv("PGHOST")
				os.Setenv("PGHOST", "foo")
				defer os.Setenv("PGHOST", old)

				dbConnector := NewDBConn("", 5432, "testdb")
				connectorStruct := dbConnector.(*GPDBConnector)
				Expect(connectorStruct.host).To(Equal("foo"))
			})
		})
		Context("when host parameter is empty and PGHOST is empty", func() {
			It("uses localhost", func() {
				old := os.Getenv("PGHOST")
				os.Setenv("PGHOST", "")
				defer os.Setenv("PGHOST", old)

				dbConnector := NewDBConn("", 5432, "")
				connectorStruct := dbConnector.(*GPDBConnector)
				_, _, currentHost := utils.GetUserAndHostInfo()
				Expect(connectorStruct.host).To(Equal(currentHost))
			})
		})
		Context("when port parameter is 0 and PGPORT is set", func() {
			It("uses PGPORT", func() {
				old := os.Getenv("PGPORT")
				os.Setenv("PGPORT", "777")
				defer os.Setenv("PGPORT", old)

				dbConnector := NewDBConn("", 0, "")
				connectorStruct := dbConnector.(*GPDBConnector)
				Expect(connectorStruct.port).To(Equal(777))
			})
		})
		Context("when port parameter is 0 and PGPORT is not set", func() {
			It("uses 15432", func() {
				old := os.Getenv("PGPORT")
				os.Setenv("PGPORT", "")
				defer os.Setenv("PGPORT", old)

				dbConnector := NewDBConn("", 0, "")
				connectorStruct := dbConnector.(*GPDBConnector)
				Expect(connectorStruct.port).To(Equal(15432))
			})
		})
	})
	Describe("#Connect", func() {
		Context("when the database exists", func() {
			It("connects successfully", func() {
				var mockDBConn DBConnector
				mockDBConn, _ = CreateMockDBConn("localhost", 5432)
				err := mockDBConn.Connect()
				defer mockDBConn.Close()
				Expect(err).ToNot(HaveOccurred())
			})
		})
		Context("when the database does not exist", func() {
			It("fails", func() {
				mockdb, _ := CreateMockDB()
				gpdbConnector := &GPDBConnector{
					driver: testutils.TestDriver{DBExists: false, DB: mockdb, DBName: "testdb"},
					dbName: "testdb",
					host:   "localhost",
					port:   5432,
				}
				err := gpdbConnector.Connect()
				Expect(err).To(HaveOccurred())
			})
		})
	})
})
