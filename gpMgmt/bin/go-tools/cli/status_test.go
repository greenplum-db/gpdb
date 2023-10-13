package cli_test

import (
	"errors"
	"strings"
	"testing"

	"github.com/golang/mock/gomock"
	"github.com/greenplum-db/gpdb/gp/cli"
	"github.com/greenplum-db/gpdb/gp/hub"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/idl/mock_idl"
	"github.com/greenplum-db/gpdb/gp/testutils"
	"github.com/greenplum-db/gpdb/gp/utils"
)

func TestPrintServicesStatus(t *testing.T) {
	setupTest(t)
	defer teardownTest()

	t.Run("returns no error when there's none", func(t *testing.T) {
		defer resetCLIVars()
		cli.ShowHubStatus = func(conf *hub.Config, skipHeader bool) (bool, error) {
			return true, nil
		}
		cli.ShowAgentsStatus = func(conf *hub.Config, skipHeader bool) error {
			return nil
		}

		err := cli.PrintServicesStatus()
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})
	t.Run("returns an error when error printing Hub status", func(t *testing.T) {
		defer resetCLIVars()
		expectedStr := "TEST Error printing Hub status"
		cli.ShowHubStatus = func(conf *hub.Config, skipHeader bool) (bool, error) {
			return false, errors.New(expectedStr)
		}
		err := cli.PrintServicesStatus()
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})
	t.Run("returns an error when error printing Agent status", func(t *testing.T) {
		defer resetCLIVars()
		expectedStr := "TEST Error printing Agent status"
		cli.ShowHubStatus = func(conf *hub.Config, skipHeader bool) (bool, error) {
			return true, nil
		}
		cli.ShowAgentsStatus = func(conf *hub.Config, skipHeader bool) error {
			return errors.New(expectedStr)
		}

		err := cli.PrintServicesStatus()
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})
}

func TestRunServiceStatus(t *testing.T) {
	setupTest(t)
	defer teardownTest()

	t.Run("returns no error when there is none", func(t *testing.T) {
		defer resetCLIVars()
		cli.PrintServicesStatus = funcNilError()

		err := cli.RunServiceStatus(nil, nil)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})
	t.Run("returns error when print service status fails", func(t *testing.T) {
		expectedStr := "TEST Error printing service status"
		defer resetCLIVars()
		cli.PrintServicesStatus = func() error {
			return errors.New(expectedStr)
		}

		err := cli.RunServiceStatus(nil, nil)
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})
}

func TestShowAgentStatus(t *testing.T) {
	setupTest(t)
	defer teardownTest()

	t.Run("returns no error when there's none with  header", func(t *testing.T) {
		defer resetCLIVars()
		cli.ConnectToHub = func(conf *hub.Config) (idl.HubClient, error) {
			hubClient := mock_idl.NewMockHubClient(ctrl)
			hubClient.EXPECT().StatusAgents(gomock.Any(), gomock.Any()).Return(&idl.StatusAgentsReply{}, nil)
			return hubClient, nil
		}

		err := cli.ShowAgentsStatus(cli.Conf, false)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})
	t.Run("returns no error when there's none with no header", func(t *testing.T) {
		defer resetCLIVars()
		cli.ConnectToHub = func(conf *hub.Config) (idl.HubClient, error) {
			hubClient := mock_idl.NewMockHubClient(ctrl)
			hubClient.EXPECT().StatusAgents(gomock.Any(), gomock.Any()).Return(&idl.StatusAgentsReply{}, nil)
			return hubClient, nil
		}

		err := cli.ShowAgentsStatus(cli.Conf, true)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})
	t.Run("returns error when there error connecting Hub", func(t *testing.T) {
		defer resetCLIVars()
		expectedStr := "TEST Error connecting Hub"
		cli.ConnectToHub = func(conf *hub.Config) (idl.HubClient, error) {
			return nil, errors.New(expectedStr)
		}

		err := cli.ShowAgentsStatus(cli.Conf, true)
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})
}

func TestShowHubStatus(t *testing.T) {
	setupTest(t)
	defer teardownTest()

	t.Run("returns no error when there is none", func(t *testing.T) {
		mockPlatform := &testutils.MockPlatform{Err: nil}
		mockPlatform.RetStatus = &idl.ServiceStatus{Status: "Running", Uptime: "10ms", Pid: uint32(1234)}
		cli.Platform = mockPlatform
		defer func() { cli.Platform = utils.GetPlatform() }()

		_, err := cli.ShowHubStatus(cli.Conf, true)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})
	t.Run("returns error when error getting service status", func(t *testing.T) {
		expectedStr := "TEST Error getting service status"
		mockPlatform := &testutils.MockPlatform{Err: errors.New(expectedStr), ServiceStatusMessage: ""}
		mockPlatform.RetStatus = &idl.ServiceStatus{Status: "Running", Uptime: "10ms", Pid: uint32(1234)}
		cli.Platform = mockPlatform
		defer func() { cli.Platform = utils.GetPlatform() }()

		_, err := cli.ShowHubStatus(cli.Conf, true)
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})
}

func TestRunStatusAgent(t *testing.T) {
	setupTest(t)
	defer teardownTest()

	t.Run("return no error when there is none", func(t *testing.T) {
		defer resetCLIVars()
		cli.ShowAgentsStatus = func(conf *hub.Config, skipHeader bool) error {
			return nil
		}

		err := cli.RunStatusAgent(nil, nil)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})
	t.Run("return error when there is error getting agent status", func(t *testing.T) {
		defer resetCLIVars()
		expectedStr := "TEST Error getting agent status"
		cli.ShowAgentsStatus = func(conf *hub.Config, skipHeader bool) error {
			return errors.New(expectedStr)
		}

		err := cli.RunStatusAgent(nil, nil)
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})
}

func TestRunStatusHub(t *testing.T) {
	setupTest(t)
	defer teardownTest()

	t.Run("return no error when there is none", func(t *testing.T) {
		defer resetCLIVars()
		cli.ShowHubStatus = func(conf *hub.Config, skipHeader bool) (bool, error) {
			return true, nil
		}

		err := cli.RunStatusHub(nil, nil)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})
	t.Run("return error when there is error getting agent status", func(t *testing.T) {
		defer resetCLIVars()
		expectedStr := "TEST Error getting agent status"
		cli.ShowHubStatus = func(conf *hub.Config, skipHeader bool) (bool, error) {
			return false, errors.New(expectedStr)
		}

		err := cli.RunStatusHub(nil, nil)
		if !strings.Contains(err.Error(), expectedStr) {
			t.Fatalf("got %v, want %v", err, expectedStr)
		}
	})
}
