package testutils

import (
	"errors"
	"io"
	"os"
	"os/exec"
	"reflect"
	"regexp"
	"sort"
	"strings"
	"testing"

	"github.com/DATA-DOG/go-sqlmock"
	"github.com/jmoiron/sqlx"
	"github.com/onsi/gomega/gbytes"
	"google.golang.org/grpc/credentials"

	"github.com/greenplum-db/gp-common-go-libs/dbconn"
	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/constants"
	"github.com/greenplum-db/gpdb/gp/hub"
	"github.com/greenplum-db/gpdb/gp/idl"
)

type MockPlatform struct {
	RetStatus            *idl.ServiceStatus
	ServiceStatusMessage string
	Err                  error
	ServiceFileContent   string
	DefServiceDir        string
	StartCmd             *exec.Cmd
	ConfigFileData       []byte
	OS                   string
}

func InitializeTestEnv() *hub.Config {
	host, _ := os.Hostname()
	gpHome := os.Getenv("GPHOME")
	credCmd := &MockCredentials{}
	conf := &hub.Config{
		Port:        constants.DefaultHubPort,
		AgentPort:   constants.DefaultAgentPort,
		Hostnames:   []string{host},
		LogDir:      "/tmp/logDir",
		ServiceName: constants.DefaultServiceName,
		GpHome:      gpHome,
		Credentials: credCmd,
	}
	return conf
}
func (p *MockPlatform) CreateServiceDir(hostnames []string, serviceDir string, gpHome string) error {
	return nil
}
func (p *MockPlatform) GetServiceStatusMessage(serviceName string) (string, error) {
	return p.ServiceStatusMessage, p.Err
}
func (p *MockPlatform) GenerateServiceFileContents(process string, gpHome string, serviceName string) string {
	return p.ServiceFileContent
}
func (p *MockPlatform) GetDefaultServiceDir() string {
	return p.DefServiceDir
}
func (p *MockPlatform) ReloadHubService(servicePath string) error {
	return p.Err
}
func (p *MockPlatform) ReloadAgentService(gpHome string, hostList []string, servicePath string) error {
	return p.Err
}
func (p *MockPlatform) CreateAndInstallHubServiceFile(gpHome string, serviceDir string, serviceName string) error {
	return p.Err
}
func (p *MockPlatform) CreateAndInstallAgentServiceFile(hostnames []string, gpHome string, serviceDir string, serviceName string) error {
	return p.Err
}
func (p *MockPlatform) GetStartHubCommand(serviceName string) *exec.Cmd {
	return p.StartCmd
}
func (p *MockPlatform) GetStartAgentCommandString(serviceName string) []string {
	return nil
}
func (p *MockPlatform) ParseServiceStatusMessage(message string) idl.ServiceStatus {
	return idl.ServiceStatus{Status: p.RetStatus.Status, Pid: p.RetStatus.Pid, Uptime: p.RetStatus.Uptime}
}
func (p *MockPlatform) DisplayServiceStatus(outfile io.Writer, serviceName string, statuses []*idl.ServiceStatus, skipHeader bool) {
}
func (p *MockPlatform) EnableUserLingering(hostnames []string, gpHome string, serviceUser string) error {
	return nil
}
func (p *MockPlatform) ReadFile(configFilePath string) (config *hub.Config, err error) {
	return nil, err
}
func (p *MockPlatform) SetServiceFileContent(content string) {
	p.ServiceFileContent = content
}
func (p *MockPlatform) GetPlatformOS() string {
	return p.OS
}

type MockCredentials struct {
	TlsConnection credentials.TransportCredentials
	Err           error
}

func (s *MockCredentials) LoadServerCredentials() (credentials.TransportCredentials, error) {
	return s.TlsConnection, s.Err
}

func (s *MockCredentials) LoadClientCredentials() (credentials.TransportCredentials, error) {
	return s.TlsConnection, s.Err
}

func (s *MockCredentials) SetCredsError(errMsg string) {
	s.Err = errors.New(errMsg)
}
func (s *MockCredentials) ResetCredsError() {
	s.Err = nil
}

func AssertLogMessage(t *testing.T, buffer *gbytes.Buffer, message string) {
	t.Helper()

	pattern, err := regexp.Compile(message)
	if err != nil {
		t.Fatalf("unexpected error when compiling regex: %#v", err)
	}

	if !pattern.MatchString(string(buffer.Contents())) {
		t.Fatalf("expected pattern '%s' not found in log '%s'", message, buffer.Contents())
	}
}

func AssertFileContents(t *testing.T, filepath string, expected string) {
	t.Helper()

	result, err := os.ReadFile(filepath)
	if err != nil {
		t.Fatalf("unexpected error: %#v", err)
	}

	if strings.TrimSpace(string(result)) != strings.TrimSpace(expected) {
		t.Fatalf("got %s, want %s", result, expected)
	}
}

func AssertFileContentsUnordered(t *testing.T, filepath string, expected string) {
	t.Helper()

	result, err := os.ReadFile(filepath)
	if err != nil {
		t.Fatalf("unexpected error: %#v", err)
	}

	lines := strings.Split(strings.TrimSpace(string(result)), "\n")
	expectedLines := strings.Split(strings.TrimSpace(expected), "\n")

	sort.Strings(lines)
	sort.Strings(expectedLines)

	if !reflect.DeepEqual(lines, expectedLines) {
		t.Fatalf("got %s, want %s", result, expected)
	}
}

func CreateMockDB() (*sqlx.DB, sqlmock.Sqlmock, error) {
	db, mock, err := sqlmock.New()
	mockdb := sqlx.NewDb(db, "sqlmock")
	if err != nil {
		return nil, nil, err
	}

	return mockdb, mock, nil
}

func CreateMockDBConn() (*dbconn.DBConn, sqlmock.Sqlmock, error) {
	mockdb, mock, err := CreateMockDB()
	if err != nil {
		return nil, nil, err
	}

	driver := &testhelper.TestDriver{DB: mockdb, DBName: "template1", User: "testrole"}
	connection := dbconn.NewDBConnFromEnvironment("template1")
	connection.Driver = driver
	connection.Host = "testhost"
	connection.Port = 5432

	return connection, mock, nil
}

func CreateAndConnectMockDB(numConns int) (*dbconn.DBConn, sqlmock.Sqlmock, error) {
	connection, mock, err := CreateMockDBConn()
	if err != nil {
		return nil, nil, err
	}

	testhelper.ExpectVersionQuery(mock, "7.0.0")
	connection.MustConnect(numConns)

	return connection, mock, nil
}
