package commands

import (
	"database/sql/driver"

	"errors"

	"gp_upgrade/db"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"github.com/onsi/gomega/gbytes"
	"gopkg.in/DATA-DOG/go-sqlmock.v1"
)

var _ = Describe("version tests", func() {
	var subject CheckVersionCommand
	BeforeEach(func() {
		subject = CheckVersionCommand{}
	})

	Describe("check version", func() {
		Describe("happy", func() {

			It("prints version check passed", func() {

				header := []string{"version"}
				versionRow := []driver.Value{"PostgreSQL 8.3.23 (Greenplum Database 5.0.0-alpha.4+dev.105.g342415a7dc build dev) on x86_64-apple-darwin16.5.0, compiled by GCC Apple LLVM version 8.1.0 (clang-802.0.42) compiled on Jun  8 2017 17:30:28"}

				dbConnector, mock := db.CreateMockDBConn()

				fakeResult := sqlmock.NewRows(header).AddRow(versionRow...)
				mock.ExpectQuery("SELECT version()").WillReturnRows(fakeResult)
				buffer := gbytes.NewBuffer()

				subject.execute(dbConnector, buffer)

				buffer.Close()
				Expect(dbConnector.GetConn().Stats().OpenConnections).To(Equal(0))
				Expect(string(buffer.Contents())).To(ContainSubstring(`gp_upgrade: Version Compatibility Check [OK]`))
			})
		})
		Describe("errors", func() {
			Describe("when the database version does not meet the minimum required", func() {

				It("prints version check failed", func() {

					header := []string{"version"}
					versionRow := []driver.Value{"PostgreSQL 8.3.23 (Greenplum Database 4.0.0.15 build dev) on x86_64-apple-darwin16.5.0, compiled by GCC Apple LLVM version 8.1.0 (clang-802.0.42) compiled on Jun  8 2017 17:30:28"}

					dbConnector, mock := db.CreateMockDBConn()

					fakeResult := sqlmock.NewRows(header).AddRow(versionRow...)
					mock.ExpectQuery("SELECT version()").WillReturnRows(fakeResult)
					buffer := gbytes.NewBuffer()

					subject.execute(dbConnector, buffer)

					Expect(dbConnector.GetConn().Stats().OpenConnections).To(Equal(0))
					Expect(string(buffer.Contents())).To(ContainSubstring(`gp_upgrade: Version Compatibility Check [Failed]`))
				})

			})
			Describe("when the query fails", func() {

				It("returns an error", func() {
					dbConnector, mock := db.CreateMockDBConn()

					mock.ExpectQuery("SELECT version()").WillReturnError(errors.New("the query has failed"))
					err := subject.execute(dbConnector, nil)

					Expect(dbConnector.GetConn().Stats().OpenConnections).To(Equal(0))
					Expect(err).To(HaveOccurred())
				})
			})
			Describe("when the db dbConn fails", func() {
				It("returns an error", func() {
					fakeDbConnector := FailingDbConnector{}
					err := subject.execute(fakeDbConnector, nil)

					Expect(err).To(HaveOccurred())
					Expect(err.Error()).To(ContainSubstring("Invalid DB Connection"))
				})
			})
		})
	})
})
