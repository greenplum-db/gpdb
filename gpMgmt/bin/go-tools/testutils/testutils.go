package testutils

import (
	"errors"
	"io"
	"os"
	"os/exec"

	"github.com/greenplum-db/gpdb/gp/constants"
	"github.com/greenplum-db/gpdb/gp/hub"
	"github.com/greenplum-db/gpdb/gp/idl"
	"google.golang.org/grpc/credentials"
)

type MockPlatform struct {
	RetStatus            *idl.ServiceStatus
	ServiceStatusMessage string
	Err                  error
	ServiceFileContent   string
	DefServiceDir        string
	StartCmd             *exec.Cmd
	ConfigFileData       []byte
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
func (p *MockPlatform) CreateServiceDir(hostnames []string, serviceDir string, gphome string) error {
	return nil
}
func (p *MockPlatform) GetServiceStatusMessage(serviceName string) (string, error) {
	return p.ServiceStatusMessage, p.Err
}
func (p *MockPlatform) GenerateServiceFileContents(process string, gphome string, serviceName string) string {
	return p.ServiceFileContent
}
func (p *MockPlatform) GetDefaultServiceDir() string {
	return p.DefServiceDir
}
func (p *MockPlatform) ReloadHubService(servicePath string) error {
	return p.Err
}
func (p *MockPlatform) ReloadAgentService(gphome string, hostList []string, servicePath string) error {
	return p.Err
}
func (p *MockPlatform) CreateAndInstallHubServiceFile(gphome string, serviceDir string, serviceName string) error {
	return p.Err
}
func (p *MockPlatform) CreateAndInstallAgentServiceFile(hostnames []string, gphome string, serviceDir string, serviceName string) error {
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
func (p *MockPlatform) EnableUserLingering(hostnames []string, gphome string, serviceUser string) error {
	return nil
}
func (p *MockPlatform) ReadFile(configFilePath string) (config *hub.Config, err error) {
	return nil, err
}
func (p *MockPlatform) SetServiceFileContent(content string) {
	p.ServiceFileContent = content
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
