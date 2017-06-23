package commands

import (
	"database/sql/driver"
	"errors"
	"gp_upgrade/db"
	"gp_upgrade/test_utils"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"github.com/onsi/gomega/gbytes"
	"gopkg.in/DATA-DOG/go-sqlmock.v1"
)

var _ = Describe("version tests", func() {
	var mock sqlmock.Sqlmock
	var dbConn *db.DBConn
	var subject CheckVersionCommand
	BeforeEach(func() {
		dbConn, mock = test_utils.CreateMockDBConn("localhost", 5432)
		subject = CheckVersionCommand{}
	})

	Describe("check version", func() {
		Describe("happy", func() {

			It("prints version check passed", func() {

				header := []string{"version"}
				versionRow := []driver.Value{"PostgreSQL 8.3.23 (Greenplum Database 5.0.0-alpha.4+dev.105.g342415a7dc build dev) on x86_64-apple-darwin16.5.0, compiled by GCC Apple LLVM version 8.1.0 (clang-802.0.42) compiled on Jun  8 2017 17:30:28"}

				fakeResult := sqlmock.NewRows(header).AddRow(versionRow...)
				mock.ExpectQuery("SELECT version()").WillReturnRows(fakeResult)
				buffer := gbytes.NewBuffer()

				subject.execute(dbConn, buffer)

				buffer.Close()
				Expect(dbConn.Conn.Stats().OpenConnections).To(Equal(0))
				Expect(string(buffer.Contents())).To(ContainSubstring(`gp_upgrade: Version Compatibility Check [OK]`))
			})
		})

		Describe("errors", func() {
			Describe("when the database version does not meet the minimum required", func() {

				It("prints version check failed", func() {

					header := []string{"version"}
					versionRow := []driver.Value{"PostgreSQL 8.3.23 (Greenplum Database 4.0.0.15 build dev) on x86_64-apple-darwin16.5.0, compiled by GCC Apple LLVM version 8.1.0 (clang-802.0.42) compiled on Jun  8 2017 17:30:28"}

					fakeResult := sqlmock.NewRows(header).AddRow(versionRow...)
					mock.ExpectQuery("SELECT version()").WillReturnRows(fakeResult)
					buffer := gbytes.NewBuffer()

					subject.execute(dbConn, buffer)

					Expect(dbConn.Conn.Stats().OpenConnections).To(Equal(0))
					Expect(string(buffer.Contents())).To(ContainSubstring(`gp_upgrade: Version Compatibility Check [Failed]`))
				})

			})
			Describe("when the query fails", func() {

				It("returns an error", func() {
					mock.ExpectQuery("SELECT version()").WillReturnError(errors.New("the query has failed"))
					err := subject.execute(dbConn, nil)

					Expect(dbConn.Conn.Stats().OpenConnections).To(Equal(0))
					Expect(err).To(HaveOccurred())
				})
			})
			Describe("when the db dbConn fails", func() {
				XIt("returns an error", func() {
					// Pending because it fails if a real Greenplum is not running. Turn back on when DBConn.Connect() is more directly mock-able
					// This codepath currently calls the public method Execute() but based on team discussion, there's no particular reason for that
					subject.Database_name = "invalidDBthatnobodywouldeverhave"
					err := subject.Execute(nil)

					Expect(err).To(HaveOccurred())
					Expect(err.Error()).To(ContainSubstring("pq: database \"invalidDBthatnobodywouldeverhave\" does not exist"))
				})
			})
		})
	})
})
