package agent_test

import (
	"errors"
	"fmt"
	"net"
	"reflect"
	"strings"
	"testing"
	"time"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	agent "github.com/greenplum-db/gpdb/gp/agent"
	"github.com/greenplum-db/gpdb/gp/constants"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/testutils"
)

func TestStartServer(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("successfully starts the server", func(t *testing.T) {
		credentials := &testutils.MockCredentials{}
		agentServer := agent.New(agent.Config{
			Port:        constants.DefaultAgentPort,
			ServiceName: constants.DefaultServiceName,
			Credentials: credentials,
		})
		errChan := make(chan error, 1)

		go func() {
			errChan <- agentServer.Start()
		}()
		defer agentServer.Shutdown()

		select {
		case err := <-errChan:
			if err != nil {
				t.Fatalf("unexpected error: %#v", err)
			}
		case <-time.After(1 * time.Second):
			t.Log("server started listening")
		}
	})

	t.Run("failed to start if the load credential fail", func(t *testing.T) {
		expected := errors.New("error")
		credentials := &testutils.MockCredentials{
			Err: expected,
		}
		agentServer := agent.New(agent.Config{
			Port:        constants.DefaultAgentPort,
			ServiceName: constants.DefaultServiceName,
			Credentials: credentials,
		})
		errChan := make(chan error, 1)

		go func() {
			errChan <- agentServer.Start()
		}()
		defer agentServer.Shutdown()

		select {
		case err := <-errChan:
			if !errors.Is(err, expected) {
				t.Fatalf("got %v, want %v", err, expected)
			}
		case <-time.After(1 * time.Second):
			t.Fatalf("expected server start to fail")
		}
	})

	t.Run("listen fails when starting the server", func(t *testing.T) {
		credentials := &testutils.MockCredentials{}

		listener, err := net.Listen("tcp", fmt.Sprintf("0.0.0.0:%d", constants.DefaultAgentPort))
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		defer listener.Close()

		agentServer := agent.New(agent.Config{
			Port:        constants.DefaultAgentPort,
			ServiceName: constants.DefaultServiceName,
			Credentials: credentials,
		})
		errChan := make(chan error, 1)

		go func() {
			errChan <- agentServer.Start()
		}()
		defer agentServer.Shutdown()

		select {
		case err := <-errChan:
			expected := fmt.Sprintf("could not listen on port %d:", constants.DefaultAgentPort)
			if !strings.Contains(err.Error(), expected) {
				t.Fatalf("got %v, want %v", err, expected)
			}
		case <-time.After(1 * time.Second):
			t.Log("expected server start to fail")
		}
	})
}

func TestGetStatus(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("returns appropriate status when no agent is running", func(t *testing.T) {
		credentials := &testutils.MockCredentials{}
		agentServer := agent.New(agent.Config{
			Port:        constants.DefaultAgentPort,
			ServiceName: constants.DefaultServiceName,
			Credentials: credentials,
		})

		result, err := agentServer.GetStatus()
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		expected := &idl.ServiceStatus{
			Status: "not running",
			Pid:    0,
			Uptime: "",
		}
		if !reflect.DeepEqual(result, expected) {
			t.Fatalf("got %+v, want %+v", result, expected)
		}
	})

	t.Run("service status returns running and uptime when hub and agent is running", func(t *testing.T) {
		credentials := &testutils.MockCredentials{}
		agentServer := agent.New(agent.Config{
			Port:        constants.DefaultAgentPort,
			ServiceName: constants.DefaultServiceName,
			Credentials: credentials,
		})

		expected := &idl.ServiceStatus{
			Status: "Running",
			Uptime: "10ms",
			Pid:    uint32(1234),
		}
		platform := &testutils.MockPlatform{
			RetStatus: expected,
			Err:       nil,
		}
		agent.SetPlatform(platform)
		defer agent.ResetPlatform()

		result, err := agentServer.GetStatus()
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		if !reflect.DeepEqual(result, expected) {
			t.Fatalf("got %+v, want %+v", result, expected)
		}
	})

	t.Run("get service status when raised error", func(t *testing.T) {
		credentials := &testutils.MockCredentials{}
		agentServer := agent.New(agent.Config{
			Port:        constants.DefaultHubPort,
			ServiceName: constants.DefaultServiceName,
			Credentials: credentials,
		})

		expected := errors.New("error")
		platform := &testutils.MockPlatform{
			Err: expected,
		}
		agent.SetPlatform(platform)
		defer agent.ResetPlatform()

		_, err := agentServer.GetStatus()
		if !errors.Is(err, expected) {
			t.Fatalf("got %v, want %v", err, expected)
		}
	})
}
