package hub_test

import (
	"context"
	"encoding/json"
	"errors"
	"log"
	"net"
	"os"
	"reflect"
	"sort"
	"strings"
	"testing"
	"time"

	"github.com/golang/mock/gomock"
	"google.golang.org/grpc"
	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/connectivity"
	"google.golang.org/grpc/credentials/insecure"
	"google.golang.org/grpc/status"
	"google.golang.org/grpc/test/bufconn"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/agent"
	"github.com/greenplum-db/gpdb/gp/constants"
	"github.com/greenplum-db/gpdb/gp/hub"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/idl/mock_idl"
	"github.com/greenplum-db/gpdb/gp/testutils"
	"github.com/greenplum-db/gpdb/gp/testutils/exectest"
	"github.com/greenplum-db/gpdb/gp/utils"
)

func init() {
	exectest.RegisterMains()
}

// Enable exectest.NewCommand mocking.
func TestMain(m *testing.M) {
	os.Exit(exectest.Run(m))
}

func TestStartServer(t *testing.T) {

	testhelper.SetupTestLogger()
	host, _ := os.Hostname()
	gpHome := os.Getenv("GPHOME")

	t.Run("successfully starts the hub server", func(t *testing.T) {

		credentials := &testutils.MockCredentials{}

		hubConfig := &hub.Config{
			1234,
			8080,
			[]string{host},
			"/tmp/logDir",
			"gp",
			gpHome,
			credentials,
		}

		hubServer := hub.New(hubConfig, nil)

		errChan := make(chan error, 1)
		go func() {
			errChan <- hubServer.Start()
		}()

		defer hubServer.Shutdown()

		select {
		case err := <-errChan:
			if err != nil {
				t.Fatalf("unexpected error: %#v", err)
			}
		case <-time.After(1 * time.Second):
			t.Log("hub server started listening")
		}

	})

	t.Run("failed to start if the load credential fail", func(t *testing.T) {
		expected := errors.New("error")
		credentials := &testutils.MockCredentials{
			Err: expected,
		}

		hubConfig := &hub.Config{
			1235,
			8080,
			[]string{host},
			"/tmp/logDir",
			"gp",
			gpHome,
			credentials,
		}
		hubServer := hub.New(hubConfig, nil)

		errChan := make(chan error, 1)
		go func() {
			errChan <- hubServer.Start()
		}()
		defer hubServer.Shutdown()

		select {
		case err := <-errChan:
			if !errors.Is(err, expected) {
				t.Fatalf("want \"Could not load credentials\" but get: %q", err.Error())
			}
		case <-time.After(1 * time.Second):
			t.Fatalf("Failed to raise error if load credential fail")
		}
	})
}

func TestStartAgents(t *testing.T) {
	testhelper.SetupTestLogger()

	credentials := &testutils.MockCredentials{TlsConnection: insecure.NewCredentials()}
	listener := bufconn.Listen(1024 * 1024)

	agentServer := grpc.NewServer()
	defer agentServer.Stop()

	idl.RegisterAgentServer(agentServer, &agent.Server{})
	go func() {
		if err := agentServer.Serve(listener); err != nil {
			log.Fatalf("server exited with error: %v", err)
		}
	}()

	hubConfig := &hub.Config{
		constants.DefaultHubPort,
		constants.DefaultAgentPort,
		[]string{"sdw1", "sdw2"},
		"/tmp/logDir",
		"gp",
		"gphome",
		credentials,
	}

	t.Run("successfully starts the agents from hub", func(t *testing.T) {
		dialer := func(ctx context.Context, address string) (net.Conn, error) {
			return listener.Dial()
		}

		hubServer := hub.New(hubConfig, dialer)

		hub.SetExecCommand(exectest.NewCommand(exectest.Success))
		defer hub.ResetExecCommand()

		_, err := hubServer.StartAgents(context.Background(), &idl.StartAgentsRequest{})
		if err != nil {
			t.Fatalf("%v", err)
		}
	})
}

func TestDialAllAgents(t *testing.T) {
	testhelper.SetupTestLogger()

	credentials := &testutils.MockCredentials{TlsConnection: insecure.NewCredentials()}
	listener := bufconn.Listen(1024 * 1024)

	agentServer := grpc.NewServer()
	defer agentServer.Stop()

	idl.RegisterAgentServer(agentServer, &agent.Server{})
	go func() {
		if err := agentServer.Serve(listener); err != nil {
			log.Fatalf("server exited with error: %v", err)
		}
	}()

	hubConfig := &hub.Config{
		constants.DefaultHubPort,
		constants.DefaultAgentPort,
		[]string{"sdw1", "sdw2"},
		"/tmp/logDir",
		"gp",
		"gphome",
		credentials,
	}

	t.Run("successfully establishes connections to agent hosts and errors out when some of the connections are not ready", func(t *testing.T) {

		dialer := func(ctx context.Context, address string) (net.Conn, error) {
			return listener.Dial()
		}

		hubServer := hub.New(hubConfig, dialer)
		err := hubServer.DialAllAgents()
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		connectedHosts := []string{}
		for _, conn := range hubServer.Conns {
			connectedHosts = append(connectedHosts, conn.Hostname)
		}
		sort.Strings(connectedHosts)

		expectedHosts := []string{"sdw1", "sdw2"}
		if !reflect.DeepEqual(connectedHosts, expectedHosts) {
			t.Fatalf("got %+v, want %+v", connectedHosts, expectedHosts)
		}

		expectedState := connectivity.Ready
		for _, conn := range hubServer.Conns {
			if conn.Conn.GetState() != expectedState {
				t.Fatalf("unexpected connection state: got %v, want %v", conn.Conn.GetState(), expectedState)
			}
		}

		// close one of the connections
		for _, conn := range hubServer.Conns {
			if conn.Hostname == "sdw2" {
				conn.Conn.Close()
			}
		}

		err = hubServer.DialAllAgents()
		expectedErr := "could not ensure connections were ready: unready hosts: sdw2"
		if err.Error() != expectedErr {
			t.Fatalf("got %v, want %v", err.Error(), expectedErr)
		}
	})

	t.Run("errors out when connection to some of the agent hosts fail", func(t *testing.T) {
		dialer := func(ctx context.Context, address string) (net.Conn, error) {
			if strings.HasPrefix(address, "sdw2") {
				return nil, errors.New("error")
			}

			return listener.Dial()
		}

		hubServer := hub.New(hubConfig, dialer)
		err := hubServer.DialAllAgents()
		expectedErr := "could not connect to agent on host sdw2:"
		if !strings.HasPrefix(err.Error(), expectedErr) {
			t.Fatalf("got %s, want %s", err.Error(), expectedErr)
		}
	})
}

func TestStatusAgents(t *testing.T) {
	testhelper.SetupTestLogger()

	credentials := &testutils.MockCredentials{TlsConnection: insecure.NewCredentials()}
	hubConfig := &hub.Config{
		constants.DefaultHubPort,
		constants.DefaultAgentPort,
		[]string{"sdw1", "sdw2"},
		"/tmp/logDir",
		"gp",
		"gphome",
		credentials,
	}
	hubServer := hub.New(hubConfig, nil)

	hub.SetEnsureConnectionsAreReady(func(conns []*hub.Connection) error {
		return nil
	})
	defer hub.ResetEnsureConnectionsAreReady()

	t.Run("gets the status from the agent hosts", func(t *testing.T) {
		ctrl := gomock.NewController(t)
		defer ctrl.Finish()

		sdw1 := mock_idl.NewMockAgentClient(ctrl)
		sdw1.EXPECT().Status(
			gomock.Any(),
			&idl.StatusAgentRequest{},
			gomock.Any(),
		).Return(&idl.StatusAgentReply{
			Status: "running",
			Uptime: "5H",
			Pid:    123,
		}, nil)

		sdw2 := mock_idl.NewMockAgentClient(ctrl)
		sdw2.EXPECT().Status(
			gomock.Any(),
			&idl.StatusAgentRequest{},
			gomock.Any(),
		).Return(&idl.StatusAgentReply{
			Status: "running",
			Uptime: "2H",
			Pid:    456,
		}, nil)

		agentConns := []*hub.Connection{
			{AgentClient: sdw1, Hostname: "sdw1"},
			{AgentClient: sdw2, Hostname: "sdw2"},
		}
		hubServer.Conns = agentConns

		result, err := hubServer.StatusAgents(context.Background(), &idl.StatusAgentsRequest{})
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		expected := &idl.StatusAgentsReply{
			Statuses: []*idl.ServiceStatus{
				{Host: "sdw2", Status: "running", Uptime: "2H", Pid: 456},
				{Host: "sdw1", Status: "running", Uptime: "5H", Pid: 123},
			},
		}
		if !reflect.DeepEqual(result, expected) {
			t.Fatalf("got %+v, want %+v", result, expected)
		}
	})

	t.Run("errors out when not able to get the status from one of the hosts", func(t *testing.T) {
		ctrl := gomock.NewController(t)
		defer ctrl.Finish()

		sdw1 := mock_idl.NewMockAgentClient(ctrl)
		sdw1.EXPECT().Status(
			gomock.Any(),
			&idl.StatusAgentRequest{},
			gomock.Any(),
		).Return(&idl.StatusAgentReply{
			Status: "running",
			Uptime: "5H",
			Pid:    123,
		}, nil)

		sdw2 := mock_idl.NewMockAgentClient(ctrl)
		sdw2.EXPECT().Status(
			gomock.Any(),
			&idl.StatusAgentRequest{},
			gomock.Any(),
		).Return(&idl.StatusAgentReply{
			Status: "running",
			Uptime: "2H",
			Pid:    456,
		}, errors.New("error"))

		agentConns := []*hub.Connection{
			{AgentClient: sdw1, Hostname: "sdw1"},
			{AgentClient: sdw2, Hostname: "sdw2"},
		}
		hubServer.Conns = agentConns

		_, err := hubServer.StatusAgents(context.Background(), &idl.StatusAgentsRequest{})
		expectedErr := "failed to get agent status on host sdw2"
		if err.Error() != expectedErr {
			t.Fatalf("got %v, want %v", err, expectedErr)
		}
	})
}

func TestStopAgents(t *testing.T) {
	testhelper.SetupTestLogger()

	credentials := &testutils.MockCredentials{TlsConnection: insecure.NewCredentials()}
	hubConfig := &hub.Config{
		constants.DefaultHubPort,
		constants.DefaultAgentPort,
		[]string{"sdw1", "sdw2"},
		"/tmp/logDir",
		"gp",
		"gphome",
		credentials,
	}
	hubServer := hub.New(hubConfig, nil)

	hub.SetEnsureConnectionsAreReady(func(conns []*hub.Connection) error {
		return nil
	})
	defer hub.ResetEnsureConnectionsAreReady()

	t.Run("successfully stops all the agents", func(t *testing.T) {
		ctrl := gomock.NewController(t)
		defer ctrl.Finish()

		sdw1 := mock_idl.NewMockAgentClient(ctrl)
		sdw1.EXPECT().Stop(
			gomock.Any(),
			&idl.StopAgentRequest{},
			gomock.Any(),
		).Return(&idl.StopAgentReply{}, status.Errorf(codes.Unavailable, ""))

		sdw2 := mock_idl.NewMockAgentClient(ctrl)
		sdw2.EXPECT().Stop(
			gomock.Any(),
			&idl.StopAgentRequest{},
			gomock.Any(),
		).Return(&idl.StopAgentReply{}, status.Errorf(codes.Unavailable, ""))

		agentConns := []*hub.Connection{
			{AgentClient: sdw1, Hostname: "sdw1"},
			{AgentClient: sdw2, Hostname: "sdw2"},
		}
		hubServer.Conns = agentConns

		_, err := hubServer.StopAgents(context.Background(), &idl.StopAgentsRequest{})
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})

	t.Run("errors out when not able to stop the agents", func(t *testing.T) {
		ctrl := gomock.NewController(t)
		defer ctrl.Finish()

		sdw1 := mock_idl.NewMockAgentClient(ctrl)
		sdw1.EXPECT().Stop(
			gomock.Any(),
			&idl.StopAgentRequest{},
			gomock.Any(),
		).Return(&idl.StopAgentReply{}, status.Errorf(codes.Unavailable, ""))

		sdw2 := mock_idl.NewMockAgentClient(ctrl)
		sdw2.EXPECT().Stop(
			gomock.Any(),
			&idl.StopAgentRequest{},
			gomock.Any(),
		).Return(&idl.StopAgentReply{}, errors.New("error"))

		agentConns := []*hub.Connection{
			{AgentClient: sdw1, Hostname: "sdw1"},
			{AgentClient: sdw2, Hostname: "sdw2"},
		}
		hubServer.Conns = agentConns

		_, err := hubServer.StopAgents(context.Background(), &idl.StopAgentsRequest{})
		expectedErr := "failed to stop agent on host sdw2: error"
		if err.Error() != expectedErr {
			t.Fatalf("got %v, want %v", err, expectedErr)
		}
	})
}

func TestConfig(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("successfully writes the config to a file and loads the same config when reading from it", func(t *testing.T) {
		file, err := os.CreateTemp("", "test")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		defer os.Remove(file.Name())

		hub.SetExecCommand(exectest.NewCommand(exectest.Success))
		defer hub.ResetExecCommand()

		expectedConfig := hub.Config{
			Port:        123,
			AgentPort:   456,
			Hostnames:   []string{"sdw1", "sdw2"},
			LogDir:      "/path/to/logdir",
			ServiceName: "gp_test",
			GpHome:      "gphome",
			Credentials: &utils.GpCredentials{
				CACertPath:     "/path/to/caCertFile",
				CAKeyPath:      "/path/to/caKeyFile",
				ServerCertPath: "/path/to/serverCertFile",
				ServerKeyPath:  "/path/to/serverKeyFile",
			},
		}
		err = expectedConfig.Write(file.Name())
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		resultConfig := hub.Config{}
		err = resultConfig.Load(file.Name())
		if err != nil {
			t.Fatalf("unexpected err: %#v", err)
		}

		if !reflect.DeepEqual(resultConfig, expectedConfig) {
			t.Fatalf("got %+v, want %+v", resultConfig, expectedConfig)
		}
	})

	t.Run("returns appropriate error when fails to write config", func(t *testing.T) {
		file, err := os.CreateTemp("", "test")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		defer os.Remove(file.Name())

		hub.SetExecCommand(exectest.NewCommand(exectest.Failure))
		defer hub.ResetExecCommand()

		config := hub.Config{
			Hostnames: []string{"sdw1", "sdw2"},
		}
		err = config.Write(file.Name())
		expectedErrPrefix := "could not copy gp.conf file to segment hosts: exit status 1"
		if !strings.HasPrefix(err.Error(), expectedErrPrefix) {
			t.Fatalf("got %v, want %v", err, expectedErrPrefix)
		}

		err = os.Chmod(file.Name(), 0000)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		err = config.Write(file.Name())
		if !errors.Is(err, os.ErrPermission) {
			t.Fatalf("got %v, want %v", err, os.ErrPermission)
		}
	})

	t.Run("returns appropriate error when fails to load config", func(t *testing.T) {
		file, err := os.CreateTemp("", "test")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		defer os.Remove(file.Name())

		_, err = file.WriteString("####")
		if err != nil {
			t.Fatalf("unexpected error: %v", err)
		}

		config := hub.Config{}
		err = config.Load(file.Name())

		var expectedErr *json.SyntaxError
		if !errors.As(err, &expectedErr) {
			t.Fatalf("got %T, want %T", err, expectedErr)
		}

		err = os.Chmod(file.Name(), 0000)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		err = config.Load(file.Name())
		if !errors.Is(err, os.ErrPermission) {
			t.Fatalf("got %#v, want %#v", err, os.ErrPermission)
		}
	})
}
