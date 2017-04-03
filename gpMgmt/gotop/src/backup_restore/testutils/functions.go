package testutils

import (
	"backup_restore/utils"
	"fmt"
	"regexp"
	"strings"

	"github.com/jmoiron/sqlx"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"github.com/onsi/gomega/gbytes"
	"gopkg.in/DATA-DOG/go-sqlmock.v1"
)

func CreateAndConnectMockDB() (*utils.DBConn, sqlmock.Sqlmock) {
	mockdb, mock := CreateMockDB()
	driver := utils.TestDriver{DBExists: true, DB: mockdb, DBName: "testdb"}
	connection := utils.NewDBConn("testdb")
	connection.Driver = driver
	connection.Connect()
	return connection, mock
}

func CreateMockDB() (*sqlx.DB, sqlmock.Sqlmock) {
	db, mock, err := sqlmock.New()
	mockdb := sqlx.NewDb(db, "sqlmock")
	if err != nil {
		Fail("Could not create mock database connection")
	}
	return mockdb, mock
}

func ExpectBegin(mock sqlmock.Sqlmock) {
	fakeResult := utils.TestResult{Rows: 0}
	mock.ExpectBegin()
	mock.ExpectExec("SET TRANSACTION(.*)").WillReturnResult(fakeResult)
}

func ExpectRegexp(buffer *gbytes.Buffer, testStr string) {
	Expect(buffer).Should(gbytes.Say(regexp.QuoteMeta(testStr)))
}

func ExpectRegex(result string, testStr string) {
	Expect(result).To(Equal(regexp.QuoteMeta(testStr)))
}

func ShouldPanicWithMessage(message string) {
	if r := recover(); r != nil {
		errMsg := strings.TrimSpace(fmt.Sprintf("%v", r))
		if errMsg != message {
			Fail(fmt.Sprintf("Expected panic message '%s', got '%s'", message, errMsg))
		}
	} else {
		Fail("Function did not panic as expected")
	}
}
