package services_test

import (
	"database/sql/driver"
	"errors"
	"gp_upgrade/db"
	"gp_upgrade/hub/services"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"github.com/onsi/gomega/gbytes"
	"gopkg.in/DATA-DOG/go-sqlmock.v1"
)

var _ bool = Describe("hub", func() {
	var (
		dbConnector db.Connector
		mock        sqlmock.Sqlmock
		testLogFile *gbytes.Buffer
	)

	BeforeEach(func() {
		dbConnector, mock = db.CreateMockDBConn()
		dbConnector.Connect()
		_, _, testLogFile = testhelper.SetupTestLogger()
	})

	AfterEach(func() {
		dbConnector.Close()
		// No controller test up into which to pull this assertion
		// So maybe look into putting assertions like this into the integration tests, so protect against leaks?
		Expect(dbConnector.GetConn().Stats().OpenConnections).
			To(Equal(0))
	})
	Describe("GetDbList", func() {
		It("returns list of db names", func() {

			mock.ExpectQuery(services.GET_DATABASE_NAMES).
				WillReturnRows(sqlmock.NewRows([]string{"datname"}).
					AddRow([]driver.Value{"template1"}...))
			names, err := services.GetDbList(dbConnector.GetConn())
			Expect(err).ToNot(HaveOccurred())
			Expect(len(names)).To(Equal(1))
			Expect(names[0]).To(Equal("template1"))
		})
		It("returns err if query fails", func() {
			mock.ExpectQuery(services.GET_DATABASE_NAMES).
				WillReturnError(errors.New("the query has failed"))
			_, err := services.GetDbList(dbConnector.GetConn())
			Expect(err).To(HaveOccurred())
			Expect(err.Error()).To(Equal("the query has failed"))
			Expect(string(testLogFile.Contents())).To(ContainSubstring("the query has failed"))
		})
	})
	Describe("GetCountsForDb", func() {
		It("returns count for AO and HEAP tables", func() {
			fakeResults := sqlmock.NewRows([]string{"count"}).
				AddRow([]driver.Value{int32(2)}...)
			mock.ExpectQuery(".*c.relstorage IN.*").
				WillReturnRows(fakeResults)

			fakeResults = sqlmock.NewRows([]string{"count"}).
				AddRow([]driver.Value{int32(3)}...)
			mock.ExpectQuery(".*c.relstorage NOT IN.*").
				WillReturnRows(fakeResults)

			aocount, heapcount, err := services.GetCountsForDb(dbConnector.GetConn())
			Expect(err).ToNot(HaveOccurred())
			Expect(aocount).To(Equal(int32(2)))
			Expect(heapcount).To(Equal(int32(3)))

		})
	})
})
