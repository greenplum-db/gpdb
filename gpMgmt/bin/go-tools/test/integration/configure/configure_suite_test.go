package configure

import (
	"flag"
	"fmt"
	"github.com/greenplum-db/gpdb/gp/utils/greenplum"
	"os"
	"testing"

	"github.com/greenplum-db/gpdb/gp/constants"
	"github.com/greenplum-db/gpdb/gp/hub"
	"github.com/greenplum-db/gpdb/gp/test/integration/testutils"
	"github.com/greenplum-db/gpdb/gp/utils"
)

const (
	defaultLogFile = "/tmp/gp_configure.log"
)

var (
	defaultServiceDir string
	serviceExt        string
	defaultGPConf     hub.Config
)

var (
	expectedOutput = []string{
		"[INFO] Created service file directory",
		"[INFO] Wrote hub service file",
		"[INFO] Wrote agent service file",
	}
	helpTxt = []string{
		"Configure gp as a systemd daemon",
		"Usage:",
		"Flags:",
		"Global Flags:",
	}
	mockHostFile = "hostlist"
	hostfile     = flag.String("hostfile", "", "file containing list of hosts")
)

func init() {
	certPath := "/tmp/certificates"
	p := utils.GetPlatform()
	defaultServiceDir, serviceExt, _ = testutils.GetServiceDetails(p)
	cred := &utils.GpCredentials{
		CACertPath:     fmt.Sprintf("%s/%s", certPath, "ca-cert.pem"),
		CAKeyPath:      fmt.Sprintf("%s/%s", certPath, "ca-key.pem"),
		ServerCertPath: fmt.Sprintf("%s/%s", certPath, "server-cert.pem"),
		ServerKeyPath:  fmt.Sprintf("%s/%s", certPath, "server-key.pem"),
	}
	defaultGPConf = hub.Config{
		Port:        constants.DefaultHubPort,
		AgentPort:   constants.DefaultAgentPort,
		Hostnames:   []string{},
		LogDir:      greenplum.GetDefaultHubLogDir(),
		ServiceName: constants.DefaultServiceName,
		GpHome:      testutils.GpHome,
		Credentials: cred,
	}
}

// TestMain function to run tests and perform cleanup at the end.
func TestMain(m *testing.M) {
	flag.Parse()
	// if hostfile is not provided as input argument, create it with default host
	if *hostfile == "" {
		*hostfile = testutils.DefaultHostfile
		_ = os.WriteFile(*hostfile, []byte(testutils.DefaultHost), 0644)
	}
	exitVal := m.Run()
	tearDownTest()

	os.Exit(exitVal)
}

func tearDownTest() {
	testutils.CleanupFilesOnHub(mockHostFile,
		fmt.Sprintf("%s/%s", testutils.GpHome, constants.ConfigFileName))
}
