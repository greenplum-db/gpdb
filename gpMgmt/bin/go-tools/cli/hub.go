package cli

import (
	"errors"
	"fmt"
	"os"
	"path/filepath"
	"strings"

	"github.com/greenplum-db/gpdb/gp/constants"
	"github.com/greenplum-db/gpdb/gp/hub"
	"github.com/greenplum-db/gpdb/gp/utils"
	"github.com/spf13/cobra"
	"github.com/spf13/viper"
)

var (
	Platform          = utils.GetPlatform()
	DefaultServiceDir = Platform.GetDefaultServiceDir()

	agentPort      int
	caCertPath     string
	caKeyPath      string
	gphome         string
	hubLogDir      string
	hubPort        int
	hostnames      []string
	hostfilePath   string
	serverCertPath string
	serverKeyPath  string
	serviceDir     string // Provide the service file's directory and name separately so users can name different files for different clusters
	serviceName    string
	serviceUser    string
)

func hubCmd() *cobra.Command {
	hubCmd := &cobra.Command{
		Use:     "hub",
		Short:   "Start a gp process in hub mode",
		Long:    "Start a gp process in hub mode",
		Hidden:  true, // Should only be invoked by systemd
		PreRunE: InitializeCommand,
		RunE:    RunHub,
	}

	return hubCmd
}

func RunHub(cmd *cobra.Command, args []string) (err error) {
	h := hub.New(Conf, nil)
	err = h.Start()
	if err != nil {
		return err
	}

	return nil
}

func configureCmd() *cobra.Command {
	configureCmd := &cobra.Command{
		Use:    "configure",
		Short:  "Configure gp as a systemd daemon",
		PreRun: InitializeLogger,
		RunE:   RunConfigure,
	}

	viper.AutomaticEnv()
	// TODO: Adding input validation
	configureCmd.Flags().IntVar(&agentPort, "agent-port", constants.DefaultAgentPort, `Port on which the agents should listen`)
	configureCmd.Flags().StringVar(&gphome, "gphome", "/usr/local/greenplum-db", `Path to GPDB installation`)
	configureCmd.Flags().IntVar(&hubPort, "hub-port", constants.DefaultHubPort, `Port on which the hub should listen`)
	configureCmd.Flags().StringVar(&hubLogDir, "log-dir", constants.DefaultHubLogDir, `Path to gp hub log directory`)
	configureCmd.Flags().StringVar(&serviceName, "service-name", constants.DefaultServiceName, `Name for the generated systemd service file`)
	configureCmd.Flags().StringVar(&serviceDir, "service-dir", fmt.Sprintf(DefaultServiceDir, os.Getenv("USER")), `Path to service file directory`)
	configureCmd.Flags().StringVar(&serviceUser, "service-user", os.Getenv("USER"), `User for whom to configure the service`)
	// TLS credentials are deliberately left blank if not provided, and need to be filled in by the user
	configureCmd.Flags().StringVar(&caCertPath, "ca-certificate", "", `Path to SSL/TLS CA certificate`)
	configureCmd.Flags().StringVar(&caKeyPath, "ca-key", "", `Path to SSL/TLS CA private key`)
	configureCmd.Flags().StringVar(&serverCertPath, "server-certificate", "", `Path to hub SSL/TLS server certificate`)
	configureCmd.Flags().StringVar(&serverKeyPath, "server-key", "", `Path to hub SSL/TLS server private key`)
	// Allow passing a hostfile for "real" use cases or a few host names for tests, but not both
	configureCmd.Flags().StringArrayVar(&hostnames, "host", []string{}, `Segment hostname`)
	configureCmd.Flags().StringVar(&hostfilePath, "hostfile", "", `Path to file containing a list of segment hostnames`)
	configureCmd.MarkFlagsMutuallyExclusive("host", "hostfile")

	viper.BindPFlag("gphome", configureCmd.Flags().Lookup("gphome")) //nolint
	gphome = viper.GetString("gphome")
	return configureCmd
}
func RunConfigure(cmd *cobra.Command, args []string) (err error) {
	if gphome == "" {
		return fmt.Errorf("not a valid gphome found\n")
	}

	// Regenerate default flag values if a custom GPHOME or username is passed
	if !cmd.Flags().Lookup("config-file").Changed {
		ConfigFilePath = filepath.Join(gphome, constants.ConfigFileName)
	}

	if cmd.Flags().Lookup("service-user").Changed && !cmd.Flags().Lookup("service-dir").Changed {
		serviceDir = fmt.Sprintf(DefaultServiceDir, serviceUser)
	}

	if !cmd.Flags().Lookup("host").Changed && !cmd.Flags().Lookup("hostfile").Changed {
		return errors.New("at least one hostname must be provided using either --host or --hostfile")
	}

	// Convert file/directory paths to absolute path before writing to gp.Conf file
	err = resolveAbsolutePaths(cmd)
	if err != nil {
		return err
	}

	if cmd.Flags().Lookup("hostfile").Changed {
		hostnames, err = GetHostnames(hostfilePath)
		if err != nil {
			return err
		}
	}
	if len(hostnames) < 1 {
		return fmt.Errorf("expected at least one host or hostlist specified")
	}
	for _, host := range hostnames {
		if len(host) < 1 {
			return fmt.Errorf("empty host name found -- please provide a valid input host name")
		}
	}
	Conf = &hub.Config{
		Port:        hubPort,
		AgentPort:   agentPort,
		Hostnames:   hostnames,
		LogDir:      hubLogDir,
		ServiceName: serviceName,
		GpHome:      gphome,
		Credentials: &utils.GpCredentials{
			CACertPath:     caCertPath,
			CAKeyPath:      caKeyPath,
			ServerCertPath: serverCertPath,
			ServerKeyPath:  serverKeyPath,
		},
	}
	err = Conf.Write(ConfigFilePath)
	if err != nil {
		return err
	}

	err = Platform.CreateServiceDir(hostnames, serviceDir, gphome)
	if err != nil {
		return err
	}

	err = Platform.CreateAndInstallHubServiceFile(gphome, serviceDir, serviceName)
	if err != nil {
		return err
	}

	err = Platform.CreateAndInstallAgentServiceFile(hostnames, gphome, serviceDir, serviceName)
	if err != nil {
		return err
	}

	err = Platform.EnableUserLingering(hostnames, gphome, serviceUser)
	if err != nil {
		return err
	}

	return nil
}

func resolveAbsolutePaths(cmd *cobra.Command) error {
	paths := []*string{&caCertPath, &caKeyPath, &serverCertPath, &serverKeyPath, &hubLogDir, &gphome}
	for _, path := range paths {
		p, err := filepath.Abs(*path)
		if err != nil {
			return fmt.Errorf("error resolving absolute path for %s: %w", *path, err)
		}
		*path = p
	}

	return nil
}

func GetHostnames(hostFilePath string) ([]string, error) {
	contents, err := ReadFile(hostFilePath)
	if err != nil {
		return []string{}, fmt.Errorf("could not read hostfile: %w", err)
	}

	return strings.Fields(string(contents)), nil
}
