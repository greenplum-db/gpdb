package cli

import (
	"context"
	"fmt"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/spf13/cobra"
	"google.golang.org/grpc/codes"
	grpcStatus "google.golang.org/grpc/status"
)

var (
	StopAgentService = StopAgentServiceFunc
	StopHubService   = StopHubServiceFunc
)

func stopCmd() *cobra.Command {
	stopCmd := &cobra.Command{
		Use:   "stop",
		Short: "Stop processes",
	}

	stopCmd.AddCommand(stopHubCmd())
	stopCmd.AddCommand(stopAgentsCmd())
	stopCmd.AddCommand(StopServicesCmd())

	return stopCmd
}

func stopHubCmd() *cobra.Command {
	stopHubCmd := &cobra.Command{
		Use:     "hub",
		Short:   "Stop hub",
		PreRunE: InitializeCommand,
		RunE:    RunStopHub,
	}

	return stopHubCmd
}

func RunStopHub(cmd *cobra.Command, args []string) error {
	err := StopHubService()
	if err != nil {
		return err
	}
	gplog.Info("Hub stopped successfully")
	if Verbose {
		ShowHubStatus(Conf, false)
	}
	return nil
}

func StopHubServiceFunc() error {
	client, err := ConnectToHub(Conf)
	if err != nil {
		return fmt.Errorf("could not connect to hub; is the hub running? Error: %v", err)
	}
	_, err = client.Stop(context.Background(), &idl.StopHubRequest{})
	// Ignore a "hub already stopped" error
	if err != nil {
		errCode := grpcStatus.Code(err)
		errMsg := grpcStatus.Convert(err).Message()
		// XXX: "transport is closing" is not documented but is needed to uniquely interpret codes.Unavailable
		// https://github.com/grpc/grpc/blob/v1.24.0/doc/statuscodes.md
		if errCode != codes.Unavailable || errMsg != "transport is closing" {
			return fmt.Errorf("could not stop hub: %w", err)
		}
	}
	return nil
}

func stopAgentsCmd() *cobra.Command {
	stopAgentsCmd := &cobra.Command{
		Use:     "agents",
		Short:   "Stop agents",
		PreRunE: InitializeCommand,
		RunE:    RunStopAgents,
	}

	return stopAgentsCmd
}

func RunStopAgents(cmd *cobra.Command, args []string) error {
	err := StopAgentService()
	if err != nil {
		return fmt.Errorf("error stopping agent service: %w", err)
	}
	gplog.Info("Agents stopped successfully")
	if Verbose {
		ShowAgentsStatus(Conf, false)
	}
	return nil
}

func StopAgentServiceFunc() error {
	client, err := ConnectToHub(Conf)
	if err != nil {
		return fmt.Errorf("could not connect to hub; is the hub running? Error: %v", err)
	}

	_, err = client.StopAgents(context.Background(), &idl.StopAgentsRequest{})
	if err != nil {
		return fmt.Errorf("could not stop agents: %w", err)
	}
	return nil
}

func StopServicesCmd() *cobra.Command {
	stopServicesCmd := &cobra.Command{
		Use:     "services",
		Short:   "Stop hub and agent services",
		PreRunE: InitializeCommand,
		RunE:    RunStopServices,
	}

	return stopServicesCmd
}

func RunStopServices(cmd *cobra.Command, args []string) error {
	err := StopAgentService()
	if err != nil {
		return err
	}
	gplog.Info("Agents stopped successfully")
	err = StopHubService()
	if err != nil {
		return err
	}
	gplog.Info("Hub stopped successfully")
	return nil
}
