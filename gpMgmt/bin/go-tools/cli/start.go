package cli

import (
	"context"
	"fmt"
	"time"

	"github.com/greenplum-db/gpdb/gp/constants"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
	"github.com/greenplum-db/gpdb/gp/hub"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/spf13/cobra"
)

func startCmd() *cobra.Command {
	startCmd := &cobra.Command{
		Use:   "start",
		Short: "Start hub, agents services",
	}

	startCmd.AddCommand(startHubCmd())
	startCmd.AddCommand(startAgentsCmd())
	startCmd.AddCommand(startServiceCmd())

	return startCmd
}

var (
	StartHubService        = StartHubServiceFunc
	RunStartHub            = RunStartHubFunc
	RunStartAgent          = RunStartAgentFunc
	StartAgentsAll         = StartAgentsAllFunc
	RunStartService        = RunStartServiceFunc
	WaitAndRetryHubConnect = WaitAndRetryHubConnectFunc
)

func startHubCmd() *cobra.Command {
	startHubCmd := &cobra.Command{
		Use:     "hub",
		Short:   "Start the hub",
		PreRunE: InitializeCommand,
		RunE:    RunStartHub,
	}

	return startHubCmd
}

func StartHubServiceFunc(serviceName string) error {
	err := Platform.GetStartHubCommand(serviceName).Run()
	if err != nil {
		return fmt.Errorf("failed to start hub service: %s Error: %w", serviceName, err)
	}

	return nil
}
func RunStartHubFunc(cmd *cobra.Command, args []string) error {
	err := StartHubService(Conf.ServiceName)
	if err != nil {
		return err
	}
	err = WaitAndRetryHubConnect()
	if err != nil {
		return err
	}
	gplog.Info("Hub %s started successfully", Conf.ServiceName)
	if Verbose {
		_, err = ShowHubStatus(Conf, true)
		if err != nil {
			return fmt.Errorf("could not retrieve hub status: %w", err)
		}
	}

	return nil
}

func startAgentsCmd() *cobra.Command {
	startAgentsCmd := &cobra.Command{
		Use:     "agents",
		Short:   "Start the agents",
		PreRunE: InitializeCommand,
		RunE:    RunStartAgent,
	}

	return startAgentsCmd
}

func startServiceCmd() *cobra.Command {
	startServicesCmd := &cobra.Command{
		Use:     "services",
		Short:   "Start hub and agent services",
		PreRunE: InitializeCommand,
		RunE:    RunStartService,
	}

	return startServicesCmd
}

func RunStartAgentFunc(cmd *cobra.Command, args []string) error {
	_, err := StartAgentsAll(Conf)
	if err != nil {
		return err
	}
	gplog.Info("Agents started successfully")
	if Verbose {
		err = ShowAgentsStatus(Conf, true)
		if err != nil {
			return fmt.Errorf("could not retrieve agent status: %w", err)
		}
	}

	return nil
}

func StartAgentsAllFunc(hubConfig *hub.Config) (idl.HubClient, error) {
	client, err := ConnectToHub(hubConfig)
	if err != nil {
		return client, err
	}

	_, err = client.StartAgents(context.Background(), &idl.StartAgentsRequest{})
	if err != nil {
		return client, fmt.Errorf("could not start agents: %w", err)
	}

	return client, nil
}

func RunStartServiceFunc(cmd *cobra.Command, args []string) error {
	//Starts Hub service followed by Agent service
	err := StartHubService(Conf.ServiceName)
	if err != nil {
		return err
	}
	err = WaitAndRetryHubConnect()
	if err != nil {
		return fmt.Errorf("error while connecting hub service: %w", err)
	}
	gplog.Info("Hub %s started successfully", Conf.ServiceName)

	// Start agents service
	_, err = StartAgentsAll(Conf)
	if err != nil {
		return fmt.Errorf("failed to start agents. Error: %w", err)
	}
	gplog.Info("Agents %s started successfully", Conf.ServiceName)
	if Verbose {
		err = PrintServicesStatus()
		if err != nil {
			return err
		}
	}

	return nil
}

func WaitAndRetryHubConnectFunc() error {
	var err error
	for try := 0; try < constants.MaxRetries; try++ {
		_, err = ConnectToHub(Conf)
		if err == nil {
			return nil
		}

		time.Sleep(time.Second / 2)
	}
	return fmt.Errorf("failed to connect to hub service. Check hub service log for details. Error: %w", err)
}
