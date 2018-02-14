package main

import (
	"fmt"
	"gp_upgrade/cli/commanders"
	"log"
	"os"
	"runtime/debug"

	pb "gp_upgrade/idl"

	gpbackupUtils "github.com/greenplum-db/gp-common-go-libs/gplog"
	_ "github.com/lib/pq"
	"github.com/pkg/errors"
	"github.com/spf13/cobra"
	"google.golang.org/grpc"
)

const (
	hubPort = "7527"
)

func main() {
	debug.SetTraceback("all")
	//empty logdir defaults to ~/gpAdminLogs
	gpbackupUtils.InitializeLogging("gp_upgrade_cli", "")

	var masterHost string
	var dbPort int
	var newClusterDbPort int
	var oldDataDir, oldBinDir, newDataDir, newBinDir string

	var cmdPrepare = &cobra.Command{
		Use:   "prepare",
		Short: "subcommands to help you get ready for a gp_upgrade",
		Long:  "subcommands to help you get ready for a gp_upgrade",
	}

	var cmdPrepareSubStartHub = &cobra.Command{
		Use:   "start-hub",
		Short: "starts the hub",
		Long:  "starts the hub",
		Run: func(cmd *cobra.Command, args []string) {
			preparer := commanders.Preparer{}
			err := preparer.StartHub()
			if err != nil {
				gpbackupUtils.Error(err.Error())
				os.Exit(1)
			}

			conn, connConfigErr := grpc.Dial("localhost:"+hubPort, grpc.WithInsecure())
			if connConfigErr != nil {
				gpbackupUtils.Error(connConfigErr.Error())
				os.Exit(1)
			}
			client := pb.NewCliToHubClient(conn)
			err = preparer.VerifyConnectivity(client)

			if err != nil {
				gpbackupUtils.Error("gp_upgrade is unable to connect via gRPC to the hub")
				gpbackupUtils.Error("%v", err)
				os.Exit(1)
			}
		},
	}

	var cmdPrepareSubShutdownClusters = &cobra.Command{
		Use:   "shutdown-clusters",
		Short: "shuts down both old and new cluster",
		Long:  "Current assumptions is both clusters exist.",
		PreRunE: func(cmd *cobra.Command, args []string) error {
			if oldBinDir == "" {
				return errors.New("the required flag '--old-bindir' was not specified")
			}
			if newBinDir == "" {
				return errors.New("the required flag '--new-bindir' was not specified")
			}
			return nil
		},
		Run: func(cmd *cobra.Command, args []string) {
			conn, connConfigErr := grpc.Dial("localhost:"+hubPort, grpc.WithInsecure())
			if connConfigErr != nil {
				gpbackupUtils.Error(connConfigErr.Error())
				os.Exit(1)
			}
			client := pb.NewCliToHubClient(conn)
			preparer := commanders.NewPreparer(client)
			err := preparer.ShutdownClusters(oldBinDir, newBinDir)
			if err != nil {
				gpbackupUtils.Error(err.Error())
				os.Exit(1)
			}
		},
	}
	cmdPrepareSubShutdownClusters.PersistentFlags().StringVar(&oldBinDir, "old-bindir", "", "install directory for old gpdb version")
	cmdPrepareSubShutdownClusters.PersistentFlags().StringVar(&newBinDir, "new-bindir", "", "install directory for new gpdb version")

	var cmdPrepareSubInitCluster = &cobra.Command{
		Use:   "init-cluster",
		Short: "inits the cluster",
		Long:  "Current assumptions is that the cluster already exists. And will only generate json config file for now.",
		PreRunE: func(cmd *cobra.Command, args []string) error {
			if newClusterDbPort == -1 {
				return errors.New("the required flag '--port' was not specified")
			}
			return nil
		},
		Run: func(cmd *cobra.Command, args []string) {
			conn, connConfigErr := grpc.Dial("localhost:"+hubPort, grpc.WithInsecure())
			if connConfigErr != nil {
				gpbackupUtils.Error(connConfigErr.Error())
				os.Exit(1)
			}
			client := pb.NewCliToHubClient(conn)
			preparer := commanders.NewPreparer(client)
			err := preparer.InitCluster(newClusterDbPort)
			if err != nil {
				gpbackupUtils.Error(err.Error())
				os.Exit(1)
			}
		},
	}
	cmdPrepareSubInitCluster.PersistentFlags().IntVar(&newClusterDbPort, "port", -1, "port for Greenplum on new master")

	var cmdStatus = &cobra.Command{
		Use:   "status",
		Short: "subcommands to show the status of a gp_upgrade",
		Long:  "subcommands to show the status of a gp_upgrade",
	}

	var cmdStatusSubUpgrade = &cobra.Command{
		Use:   "upgrade",
		Short: "the status of the upgrade",
		Long:  "the status of the upgrade",
		Run: func(cmd *cobra.Command, args []string) {
			conn, connConfigErr := grpc.Dial("localhost:"+hubPort, grpc.WithInsecure())
			if connConfigErr != nil {
				gpbackupUtils.Error(connConfigErr.Error())
				os.Exit(1)
			}
			client := pb.NewCliToHubClient(conn)
			reporter := commanders.NewReporter(client)
			err := reporter.OverallUpgradeStatus()
			if err != nil {
				gpbackupUtils.Error(err.Error())
				os.Exit(1)
			}
		},
	}

	var cmdCheck = &cobra.Command{
		Use:   "check",
		Short: "collects information and validates the target Greenplum installation can be upgraded",
		Long:  `collects information and validates the target Greenplum installation can be upgraded`,
		PreRunE: func(cmd *cobra.Command, args []string) error {
			if masterHost == "" {
				return errors.New("the required flag '--master-host' was not specified")
			}
			return nil
		},
		Args: cobra.MinimumNArgs(0),
	}

	cmdCheck.PersistentFlags().StringVar(&masterHost, "master-host", "", "host IP for master")
	cmdCheck.PersistentFlags().IntVar(&dbPort, "port", 15432, "port for Greenplum on master")
	cmdCheck.MarkFlagRequired("master-host")

	var cmdCheckSubCheckVersionCommand = &cobra.Command{
		Use:     "version",
		Short:   "validate current version is upgradable",
		Long:    `validate current version is upgradable`,
		Aliases: []string{"ver"},
		RunE: func(cmd *cobra.Command, args []string) error {
			conn, connConfigErr := grpc.Dial("localhost:"+hubPort,
				grpc.WithInsecure())
			if connConfigErr != nil {
				gpbackupUtils.Error(connConfigErr.Error())
				os.Exit(1)
			}
			client := pb.NewCliToHubClient(conn)
			return commanders.NewVersionChecker(client).Execute(masterHost, dbPort)
		},
	}

	var cmdCheckSubObjectCountCommand = &cobra.Command{
		Use:     "object-count",
		Short:   "count database objects and numeric objects",
		Long:    "count database objects and numeric objects",
		Aliases: []string{"oc"},
		RunE: func(cmd *cobra.Command, args []string) error {
			conn, connConfigErr := grpc.Dial("localhost:"+hubPort,
				grpc.WithInsecure())
			if connConfigErr != nil {
				fmt.Println(connConfigErr)
				os.Exit(1)
			}
			client := pb.NewCliToHubClient(conn)
			return commanders.NewObjectCountChecker(client).Execute(dbPort)
		},
	}

	var cmdCheckSubDiskSpaceCommand = &cobra.Command{
		Use:     "disk-space",
		Short:   "check that disk space usage is less than 80% on all segments",
		Long:    "check that disk space usage is less than 80% on all segments",
		Aliases: []string{"du"},
		RunE: func(cmd *cobra.Command, args []string) error {
			conn, connConfigErr := grpc.Dial("localhost:"+hubPort,
				grpc.WithInsecure())
			if connConfigErr != nil {
				gpbackupUtils.Error(connConfigErr.Error())
				os.Exit(1)
			}
			client := pb.NewCliToHubClient(conn)
			return commanders.NewDiskUsageChecker(client).Execute(dbPort)
		},
	}

	var cmdCheckSubConfigCommand = &cobra.Command{
		Use:   "config",
		Short: "gather cluster configuration",
		Long:  "gather cluster configuration",
		Run: func(cmd *cobra.Command, args []string) {
			conn, connConfigErr := grpc.Dial("localhost:"+hubPort,
				grpc.WithInsecure())
			if connConfigErr != nil {
				gpbackupUtils.Error(connConfigErr.Error())
				os.Exit(1)
			}
			client := pb.NewCliToHubClient(conn)
			err := commanders.NewConfigChecker(client).Execute(dbPort)
			if err != nil {
				gpbackupUtils.Error(err.Error())
				os.Exit(1)
			}
		},
	}

	var cmdCheckSubSeginstall = &cobra.Command{
		Use:   "seginstall",
		Short: "confirms that the new software is installed on all segments",
		Long: "Running this command will validate that the new software is installed on all segments, " +
			"and register successful or failed validation (available in `gp_upgrade status upgrade`)",
		Run: func(cmd *cobra.Command, args []string) {
			conn, connConfigErr := grpc.Dial("localhost:"+hubPort,
				grpc.WithInsecure())
			if connConfigErr != nil {
				gpbackupUtils.Error(connConfigErr.Error())
				os.Exit(1)
			}
			client := pb.NewCliToHubClient(conn)

			err := commanders.NewSeginstallChecker(client).Execute()
			if err != nil {
				gpbackupUtils.Error(err.Error())
				os.Exit(1)
			}

			fmt.Println("Seginstall is underway. Use command \"gp_upgrade status upgrade\" " +
				"to check its current status, and/or hub logs for possible errors.")
		},
	}

	var cmdVersion = &cobra.Command{
		Use:   "version",
		Short: "Version of gp_upgrade",
		Long:  `Version of gp_upgrade`,
		Run: func(cmd *cobra.Command, args []string) {
			fmt.Println(commanders.VersionString())
		},
	}

	var cmdUpgrade = &cobra.Command{
		Use:   "upgrade",
		Short: "starts upgrade process",
		Long:  `starts upgrade process`,
	}

	var cmdUpgradeSubConvertMaster = &cobra.Command{
		Use:   "convert-master",
		Short: "start upgrade process on master",
		Long:  `start upgrade process on master`,
		Run: func(cmd *cobra.Command, args []string) {
			conn, connConfigErr := grpc.Dial("localhost:"+hubPort,
				grpc.WithInsecure())
			if connConfigErr != nil {
				gpbackupUtils.Error(connConfigErr.Error())
				os.Exit(1)
			}

			client := pb.NewCliToHubClient(conn)
			err := commanders.NewUpgrader(client).ConvertMaster(oldDataDir, oldBinDir, newDataDir, newBinDir)
			if err != nil {
				gpbackupUtils.Error(err.Error())
				os.Exit(1)
			}
		},
	}

	var rootCmd = &cobra.Command{Use: "gp_upgrade"}

	//TODO this could be improved.
	// Also, if another command is added, the message will need to be updated.
	if len(os.Args[1:]) < 1 {
		log.Fatal("Please specify one command of: prepare, check, status or version")
	}
	// all root level
	rootCmd.AddCommand(cmdPrepare, cmdStatus, cmdCheck, cmdVersion, cmdUpgrade)

	// prepare subcommmands
	cmdPrepare.AddCommand(cmdPrepareSubStartHub)
	cmdPrepare.AddCommand(cmdPrepareSubInitCluster)
	cmdPrepare.AddCommand(cmdPrepareSubShutdownClusters)

	// status subcommands
	cmdStatus.AddCommand(cmdStatusSubUpgrade)

	// check subcommands
	cmdCheck.AddCommand(cmdCheckSubCheckVersionCommand)
	cmdCheck.AddCommand(cmdCheckSubObjectCountCommand)
	cmdCheck.AddCommand(cmdCheckSubDiskSpaceCommand)
	cmdCheck.AddCommand(cmdCheckSubConfigCommand)
	cmdCheck.AddCommand(cmdCheckSubSeginstall)

	// upgrade subcommands
	cmdUpgrade.AddCommand(cmdUpgradeSubConvertMaster)
	cmdUpgradeSubConvertMaster.PersistentFlags().StringVar(&oldDataDir, "old-datadir", "", "data directory for old gpdb version")
	cmdUpgradeSubConvertMaster.PersistentFlags().StringVar(&oldBinDir, "old-bindir", "", "install directory for old gpdb version")
	cmdUpgradeSubConvertMaster.PersistentFlags().StringVar(&newDataDir, "new-datadir", "", "data directory for new gpdb version")
	cmdUpgradeSubConvertMaster.PersistentFlags().StringVar(&newBinDir, "new-bindir", "", "install directory for new gpdb version")

	//TODO if give a subcommand that doesn't exist, we should give the user feedback

	err := rootCmd.Execute()
	if err != nil {
		// Use v to print the stack trace of an object errors.
		fmt.Printf("%+v\n", err)
		os.Exit(1)
	}
}
