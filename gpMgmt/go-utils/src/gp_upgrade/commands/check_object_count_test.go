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

var _ = Describe("object count tests", func() {
	var (
		subject        ObjectCountCommand
		heapfakeResult *sqlmock.Rows
		aofakeResult   *sqlmock.Rows
	)

	BeforeEach(func() {
		header := []string{"COUNT"}
		aoObjectCountRow := []driver.Value{5}
		heapObjectCountRow := []driver.Value{10}

		heapfakeResult = sqlmock.NewRows(header).AddRow(heapObjectCountRow...)
		aofakeResult = sqlmock.NewRows(header).AddRow(aoObjectCountRow...)
	})

	Describe("check object count", func() {
		Describe("happy", func() {
			It("prints the number of AO and heap objects in the database", func() {
				dbConnector, mock := db.CreateMockDBConn()
				mock.ExpectQuery("SELECT COUNT.*AND c.relstorage IN.*").WillReturnRows(aofakeResult)
				mock.ExpectQuery("SELECT COUNT.*AND c.relstorage NOT IN.*").WillReturnRows(heapfakeResult)
				buffer := gbytes.NewBuffer()

				err := subject.execute(dbConnector, buffer)

				buffer.Close()
				Expect(err).ToNot(HaveOccurred())
				Expect(dbConnector.GetConn().Stats().OpenConnections).To(Equal(0))
				Expect(string(buffer.Contents())).To(ContainSubstring("Number of AO objects - 5"))
				Expect(string(buffer.Contents())).To(ContainSubstring("Number of heap objects - 10"))
			})
		})

		Describe("errors", func() {
			Describe("when the query fails on AO table count", func() {

				It("returns an error", func() {
					dbConnector, mock := db.CreateMockDBConn()
					mock.ExpectQuery("SELECT COUNT.*AND c.relstorage IN.*").WillReturnError(errors.New("the query for AO table count has failed"))

					err := subject.execute(dbConnector, nil)

					Expect(dbConnector.GetConn().Stats().OpenConnections).To(Equal(0))
					Expect(err).To(HaveOccurred())
				})
			})
			Describe("when the query fails on heap-only table count", func() {

				It("returns an error", func() {
					dbConnector, mock := db.CreateMockDBConn()
					mock.ExpectQuery("SELECT COUNT.*AND c.relstorage IN.*").WillReturnRows(aofakeResult)
					mock.ExpectQuery("SELECT COUNT.*AND c.relstorage NOT IN.*").WillReturnError(errors.New("the query for heap-only table count has failed"))
					buffer := gbytes.NewBuffer()

					err := subject.execute(dbConnector, buffer)

					buffer.Close()
					Expect(dbConnector.GetConn().Stats().OpenConnections).To(Equal(0))
					Expect(err).To(HaveOccurred())
				})
			})
			Describe("when the db dbConn fails", func() {
				It("returns an error", func() {

					err := subject.execute(FailingDbConnector{}, nil)

					Expect(err).To(HaveOccurred())
					Expect(err.Error()).To(ContainSubstring("Invalid DB Connection"))
				})
			})
		})
	})
})
