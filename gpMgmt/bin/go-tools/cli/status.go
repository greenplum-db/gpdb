package cli

import (
	"context"
	"fmt"
	"os"

	"github.com/greenplum-db/gpdb/gp/hub"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/spf13/cobra"
)

var (
	ShowHubStatus       = ShowHubStatusFunc
	ShowAgentsStatus    = ShowAgentsStatusFunc
	PrintServicesStatus = PrintServicesStatusFunc
)

func statusCmd() *cobra.Command {
	statusCmd := &cobra.Command{
		Use:   "status",
		Short: "Display status",
	}

	statusCmd.AddCommand(statusHubCmd())
	statusCmd.AddCommand(statusAgentsCmd())
	statusCmd.AddCommand(statusServicesCmd())

	return statusCmd
}

func statusHubCmd() *cobra.Command {
	statusHubCmd := &cobra.Command{
		Use:     "hub",
		Short:   "Display hub status",
		PreRunE: InitializeCommand,
		RunE:    RunStatusHub,
	}

	return statusHubCmd
}

func RunStatusHub(cmd *cobra.Command, args []string) error {
	_, err := ShowHubStatus(Conf, false)
	if err != nil {
		return err
	}

	return nil
}

func statusAgentsCmd() *cobra.Command {
	statusAgentsCmd := &cobra.Command{
		Use:     "agents",
		Short:   "Display agents status",
		PreRunE: InitializeCommand,
		RunE:    RunStatusAgent,
	}

	return statusAgentsCmd
}

func statusServicesCmd() *cobra.Command {
	statusServicesCmd := &cobra.Command{
		Use:     "services",
		Short:   "Display Hub and Agent services status",
		PreRunE: InitializeCommand,
		RunE:    RunServiceStatus,
	}

	return statusServicesCmd
}

func RunStatusAgent(cmd *cobra.Command, args []string) error {
	err := ShowAgentsStatus(Conf, false)
	if err != nil {
		return err
	}

	return nil
}

func ShowHubStatusFunc(conf *hub.Config, skipHeader bool) (bool, error) {
	message, err := Platform.GetServiceStatusMessage(fmt.Sprintf("%s_hub", conf.ServiceName))
	if err != nil {
		return false, err
	}
	status := Platform.ParseServiceStatusMessage(message)
	status.Host, _ = os.Hostname()
	Platform.DisplayServiceStatus(os.Stdout, "Hub", []*idl.ServiceStatus{&status}, skipHeader)
	if status.Status == "Unknown" {
		return false, nil
	}

	return true, nil
}

func ShowAgentsStatusFunc(conf *hub.Config, skipHeader bool) error {
	client, err := ConnectToHub(conf)
	if err != nil {
		return err
	}

	reply, err := client.StatusAgents(context.Background(), &idl.StatusAgentsRequest{})
	if err != nil {
		return err
	}
	Platform.DisplayServiceStatus(os.Stdout, "Agent", reply.Statuses, skipHeader)

	return nil
}

func RunServiceStatus(cmd *cobra.Command, args []string) error {
	err := PrintServicesStatus()
	if err != nil {
		return err
	}

	return nil
}

func PrintServicesStatusFunc() error {
	// TODO: Check if Hub is down, do not check for Agents
	hubRunning, err := ShowHubStatus(Conf, false)
	if err != nil {
		return err
	}
	if !hubRunning {
		fmt.Println("Hub service not running, not able to fetch agent status.")
		return nil
	}
	err = ShowAgentsStatus(Conf, true)
	if err != nil {
		return err
	}

	return nil
}
