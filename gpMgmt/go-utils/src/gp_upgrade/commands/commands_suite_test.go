package commands

import (
	"errors"
	"testing"

	"github.com/jmoiron/sqlx"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

type FailingDbConnector struct{}

func (FailingDbConnector) Connect() error {
	return errors.New("Invalid DB Connection")
}
func (FailingDbConnector) Close() {
}
func (FailingDbConnector) GetConn() *sqlx.DB {
	return nil
}

func TestCommands(t *testing.T) {
	RegisterFailHandler(Fail)
	RunSpecs(t, "Commands Suite")
}
