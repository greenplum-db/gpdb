package commanders_test

import (
	"testing"

	// gpbackupUtils "github.com/greenplum-db/gp-common-go-libs/gplog"
	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

func TestCommands(t *testing.T) {
	RegisterFailHandler(Fail)
	RunSpecs(t, "Commanders Suite")
}

// Activate me once we start running unit tests. At that time, specify a better logging directory for unit test output
// var _ = BeforeSuite(func() {
// 	gpbackupUtils.InitializeLogging("commanders unit tests", "")
// })
