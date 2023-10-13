package cli_test

import (
	"errors"
	"strings"
	"testing"

	"github.com/greenplum-db/gpdb/gp/cli"

	"github.com/golang/mock/gomock"
	"github.com/greenplum-db/gpdb/gp/hub"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/idl/mock_idl"
)

func TestStopAgentService(t *testing.T) {
	setupTest(t)
	defer teardownTest()

	t.Run("StopAgentService stops the agent service when theres no error", func(t *testing.T) {
		defer resetCLIVars()
		cli.ConnectToHub = func(conf *hub.Config) (idl.HubClient, error) {
			hubClient := mock_idl.NewMockHubClient(ctrl)
			hubClient.EXPECT().StopAgents(gomock.Any(), gomock.Any())
			return hubClient, nil
		}

		err := cli.StopAgentService()
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})
	t.Run("StopAgentService returns an error when theres error connecting hub", func(t *testing.T) {
		defer resetCLIVars()
		expectedStr := "TEST Error connecting Hub"
		cli.ConnectToHub = func(conf *hub.Config) (idl.HubClient, error) {
			return nil, errors.New(expectedStr)
		}

		err := cli.StopAgentService()
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})
	t.Run("StopAgentService returns error when theres error stopping agents", func(t *testing.T) {
		defer resetCLIVars()
		expectedStr := "TEST Error stopping agents"
		cli.ConnectToHub = func(conf *hub.Config) (idl.HubClient, error) {
			hubClient := mock_idl.NewMockHubClient(ctrl)
			hubClient.EXPECT().StopAgents(gomock.Any(), gomock.Any()).Return(nil, errors.New(expectedStr))
			return hubClient, nil
		}
		err := cli.StopAgentService()
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})
}

func TestStopHubService(t *testing.T) {
	setupTest(t)
	defer teardownTest()

	t.Run("Stops hub when theres no error", func(t *testing.T) {
		defer resetCLIVars()
		cli.ConnectToHub = func(conf *hub.Config) (idl.HubClient, error) {
			hubClient := mock_idl.NewMockHubClient(ctrl)
			hubClient.EXPECT().Stop(gomock.Any(), gomock.Any())
			return hubClient, nil
		}

		err := cli.StopHubService()
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})

	t.Run("Stops hub when theres error connecting hub service", func(t *testing.T) {
		defer resetCLIVars()
		expectedStr := "TEST Error connecting Hub"
		cli.ConnectToHub = func(conf *hub.Config) (idl.HubClient, error) {
			return nil, errors.New(expectedStr)
		}

		err := cli.StopHubService()
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})

	t.Run("Stop returns error when there's error stopping Hub", func(t *testing.T) {
		defer resetCLIVars()
		expectedStr := "TEST Error stopping Hub"
		cli.ConnectToHub = func(conf *hub.Config) (idl.HubClient, error) {
			hubClient := mock_idl.NewMockHubClient(ctrl)
			hubClient.EXPECT().Stop(gomock.Any(), gomock.Any()).Return(nil, errors.New(expectedStr))
			return hubClient, nil
		}

		err := cli.StopHubService()
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})
}

func TestRunStopServices(t *testing.T) {
	setupTest(t)
	defer teardownTest()

	t.Run("Returns no error when there is none", func(t *testing.T) {
		defer resetCLIVars()
		cli.StopAgentService = funcNilError()
		cli.StopHubService = funcNilError()

		err := cli.RunStopServices(nil, nil)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})

	t.Run("Returns  error when error stopping Agent service", func(t *testing.T) {
		defer resetCLIVars()
		expectedStr := "TEST Error stopping Agent service"
		cli.StopAgentService = funcErrorMessage(expectedStr)

		err := cli.RunStopServices(nil, nil)
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})

	t.Run("Returns error when error stopping hub service", func(t *testing.T) {
		defer resetCLIVars()
		expectedStr := "Error stopping Hub service"
		cli.StopAgentService = funcNilError()
		cli.StopHubService = funcErrorMessage(expectedStr)

		err := cli.RunStopServices(nil, nil)
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})
}

func TestRunStopAgents(t *testing.T) {
	setupTest(t)
	defer teardownTest()

	t.Run("returns no error where there none no verbose", func(t *testing.T) {
		defer resetCLIVars()
		cli.StopAgentService = funcNilError()
		cli.Verbose = false

		err := cli.RunStopAgents(nil, nil)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

	})
	t.Run("returns no error where there none in verbose", func(t *testing.T) {
		defer resetCLIVars()
		cli.StopAgentService = funcNilError()
		cli.ShowAgentsStatus = func(conf *hub.Config, skipHeader bool) error {
			return nil
		}
		cli.Verbose = true

		err := cli.RunStopAgents(nil, nil)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})
	t.Run("returns error where there is error stopping agent service", func(t *testing.T) {
		defer resetCLIVars()
		expectedStr := "TEST Error stopping agents"
		cli.StopAgentService = funcErrorMessage(expectedStr)

		err := cli.RunStopAgents(nil, nil)
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})
}

func TestRunStopHub(t *testing.T) {
	setupTest(t)
	defer teardownTest()

	t.Run("return no error when there is none non verbose mode", func(t *testing.T) {
		defer resetCLIVars()
		cli.StopHubService = funcNilError()
		cli.Verbose = false

		err := cli.RunStopHub(nil, nil)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})
	t.Run("return no error when there is none verbose mode", func(t *testing.T) {
		defer resetCLIVars()
		cli.StopHubService = funcNilError()
		cli.ShowHubStatus = func(conf *hub.Config, skipHeader bool) (bool, error) {
			return true, nil
		}
		cli.Verbose = true

		err := cli.RunStopHub(nil, nil)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})
	t.Run("return error when there is error stoppig Hub service", func(t *testing.T) {
		defer resetCLIVars()
		expectedStr := "TEST Error while stopping Hub Service"
		cli.StopHubService = funcErrorMessage(expectedStr)
		cli.Verbose = false

		err := cli.RunStopHub(nil, nil)
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})
}
