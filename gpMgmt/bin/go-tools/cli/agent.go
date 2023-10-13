package cli

import (
	"github.com/greenplum-db/gpdb/gp/agent"
	"github.com/spf13/cobra"
)

func agentCmd() *cobra.Command {
	agentCmd := &cobra.Command{
		Use:     "agent",
		Short:   "Start a gp process in agent mode",
		Long:    "Start a gp process in agent mode",
		Hidden:  true, // Should only be invoked by systemd
		PreRunE: InitializeCommand,
		RunE:    RunAgent,
	}

	return agentCmd
}

func RunAgent(cmd *cobra.Command, args []string) (err error) {
	agentConf := agent.Config{Port: Conf.AgentPort, ServiceName: Conf.ServiceName, Credentials: Conf.Credentials}
	a := agent.New(agentConf)
	err = a.Start()
	if err != nil {
		return err
	}
	return nil
}
