package cli_test

import (
	"context"
	"errors"
	"strings"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/agent"
	"github.com/greenplum-db/gpdb/gp/cli"
	"github.com/greenplum-db/gpdb/gp/constants"
	"github.com/greenplum-db/gpdb/gp/hub"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/testutils"
	"google.golang.org/grpc"
)

var (
	ctrl *gomock.Controller
)

func setupTest(t *testing.T) {
	testhelper.SetupTestLogger()
	cli.Conf = testutils.InitializeTestEnv()
	ctrl = gomock.NewController(t)
}

func teardownTest() {
	ctrl.Finish()
}

func resetCLIVars() {
	cli.DialContextFunc = grpc.DialContext
	cli.ConnectToHub = cli.ConnectToHubFunc
	cli.StartHubService = cli.StartHubServiceFunc
	cli.WaitAndRetryHubConnect = cli.WaitAndRetryHubConnectFunc
	cli.ShowHubStatus = cli.ShowHubStatusFunc
	cli.StartAgentsAll = cli.StartAgentsAllFunc
	cli.ShowAgentsStatus = cli.ShowAgentsStatusFunc
	cli.PrintServicesStatus = cli.PrintServicesStatusFunc
	cli.StopAgentService = cli.StopAgentServiceFunc
	cli.StopHubService = cli.StopHubServiceFunc
}

func funcNilError() func() error {
	return func() error {
		return nil
	}
}
func funcErrorMessage(message string) func() error {
	return func() error {
		return errors.New(message)
	}
}

func TestConnectToHub(t *testing.T) {
	testhelper.SetupTestLogger()
	creds := &testutils.MockCredentials{}
	platform := &testutils.MockPlatform{Err: nil}
	platform.RetStatus = &idl.ServiceStatus{
		Status: "",
		Uptime: "",
		Pid:    uint32(0),
	}
	agent.SetPlatform(platform)
	defer agent.ResetPlatform()
	hostlist := []string{"sdw1", "sdw2", "sdw3"}
	config := hub.Config{
		Port:        constants.DefaultHubPort,
		AgentPort:   constants.DefaultAgentPort,
		Hostnames:   hostlist,
		LogDir:      "/tmp",
		ServiceName: constants.DefaultServiceName,
		GpHome:      "/usr/local/gpdb/",
		Credentials: creds,
	}

	t.Run("Connect to hub succeeds and no error thrown", func(t *testing.T) {
		defer resetCLIVars()
		cli.DialContextFunc = func(ctx context.Context, target string, opts ...grpc.DialOption) (conn *grpc.ClientConn, err error) {
			return &grpc.ClientConn{}, nil
		}

		_, err := cli.ConnectToHub(&config)
		if err != nil {
			t.Fatalf("unexpected error when connecting to hub: %#v", err)
		}
	})
	t.Run("Connect to hub returns error when Dial context fails", func(t *testing.T) {
		expectedErr := "TEST ERROR while dialing context"
		defer resetCLIVars()
		cli.DialContextFunc = func(ctx context.Context, target string, opts ...grpc.DialOption) (conn *grpc.ClientConn, err error) {
			return nil, errors.New(expectedErr)
		}

		_, err := cli.ConnectToHub(&config)
		if err == nil || !strings.Contains(err.Error(), expectedErr) {
			t.Fatalf("got %v, want %v", err, expectedErr)
		}
	})
	t.Run("Connect to hub returns error when load client credentials fail", func(t *testing.T) {
		defer resetCLIVars()
		cli.DialContextFunc = func(ctx context.Context, target string, opts ...grpc.DialOption) (conn *grpc.ClientConn, err error) {
			return &grpc.ClientConn{}, nil
		}
		expectedErr := "Load credentials error"
		creds.SetCredsError(expectedErr)
		defer creds.ResetCredsError()

		_, err := cli.ConnectToHub(&config)
		if err == nil || !strings.Contains(err.Error(), expectedErr) {
			t.Fatalf("got %v, want %v", err, expectedErr)
		}
	})
}
