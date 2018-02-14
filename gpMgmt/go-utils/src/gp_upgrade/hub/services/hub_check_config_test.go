package services_test

import (
	"gp_upgrade/db"
	"gp_upgrade/hub/services"
	"gp_upgrade/utils"

	"database/sql/driver"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"

	"github.com/onsi/gomega/gbytes"
	"github.com/pkg/errors"
	"gopkg.in/DATA-DOG/go-sqlmock.v1"
)

var _ = Describe("hub", func() {
	Describe("check configutils internals", func() {

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
			Expect(dbConnector.GetConn().Stats().OpenConnections).To(Equal(0))
		})

		Describe("happy: the database is running, master-host is provided, and connection is successful", func() {
			It("writes the resulting rows according to however the provided writer does it", func() {
				fakeQuery := "SELECT barCol FROM foo"
				mock.ExpectQuery(fakeQuery).WillReturnRows(getHappyFakeRows())
				successfulWriter := SuccessfulWriter{}
				err := services.SaveQueryResultToJSON(dbConnector.GetConn(), fakeQuery, &successfulWriter)

				Expect(err).ToNot(HaveOccurred())
				Expect(successfulWriter.CallsToLoad).To(Equal(1))
				Expect(successfulWriter.CallsToWrite).To(Equal(1))
			})
		})

		Describe("errors", func() {
			Describe("when the query fails", func() {
				It("returns an error", func() {
					fakeFailingQuery := `SEJECT % ofrm tabel1`
					mock.ExpectQuery(fakeFailingQuery).WillReturnError(errors.New("the query has failed"))

					err := services.SaveQueryResultToJSON(dbConnector.GetConn(), fakeFailingQuery, &SuccessfulWriter{})
					Expect(err).To(HaveOccurred())
					Expect(string(testLogFile.Contents())).To(ContainSubstring("the query has failed"))
				})
			})

			Describe("when the writer fails for any reason", func() {
				It("returns an error", func() {
					// focus on the writer failing rather than querying
					fineFakeQuery := "SELECT fooCol FROM bar"
					mock.ExpectQuery(fineFakeQuery).WillReturnRows(getHappyFakeRows())

					err := services.SaveQueryResultToJSON(dbConnector.GetConn(), fineFakeQuery, FailingWriter{})

					Expect(err).To(HaveOccurred())
					Expect(err.Error()).To(ContainSubstring("I always fail"))
					Expect(string(testLogFile.Contents())).To(ContainSubstring("I always fail"))
				})
			})
		})
	})
})

type SuccessfulWriter struct {
	CallsToLoad  int
	CallsToWrite int
}

func (w *SuccessfulWriter) Load(rows utils.RowsWrapper) error {
	w.CallsToLoad++
	return nil
}

func (w *SuccessfulWriter) Write() error {
	w.CallsToWrite++
	return nil
}

type FailingWriter struct{}

func (FailingWriter) Load(rows utils.RowsWrapper) error {
	return errors.New("I always fail")
}

func (FailingWriter) Write() error {
	return errors.New("I always fail")
}

// Construct sqlmock in-memory rows that are structured properly
func getHappyFakeRows() *sqlmock.Rows {
	header := []string{"dbid", "content", "role", "preferred_role", "mode", "status", "port",
		"hostname", "address", "datadir"}
	fakeConfigRow := []driver.Value{1, -1, 'p', 'p', 's', 'u', 15432, "mdw.local",
		"mdw.local", nil}
	fakeConfigRow2 := []driver.Value{2, 0, 'p', 'p', 's', 'u', 25432, "sdw1.local",
		"sdw1.local", nil}
	rows := sqlmock.NewRows(header)
	heapfakeResult := rows.AddRow(fakeConfigRow...).AddRow(fakeConfigRow2...)
	return heapfakeResult
}
