package main

import (
	"fmt"
	"os"

	"gp_upgrade/cli/commanders"
	pb "gp_upgrade/idl"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
	"github.com/spf13/cobra"
	"google.golang.org/grpc"
)

var masterHost string
var dbPort int
var newClusterDbPort int
var oldDataDir, oldBinDir, newDataDir, newBinDir string

var root = &cobra.Command{Use: "gp_upgrade"}

var prepare = &cobra.Command{
	Use:   "prepare",
	Short: "subcommands to help you get ready for a gp_upgrade",
	Long:  "subcommands to help you get ready for a gp_upgrade",
}

var status = &cobra.Command{
	Use:   "status",
	Short: "subcommands to show the status of a gp_upgrade",
	Long:  "subcommands to show the status of a gp_upgrade",
}

var check = &cobra.Command{
	Use:   "check",
	Short: "collects information and validates the target Greenplum installation can be upgraded",
	Long:  `collects information and validates the target Greenplum installation can be upgraded`,
}

var version = &cobra.Command{
	Use:   "version",
	Short: "Version of gp_upgrade",
	Long:  `Version of gp_upgrade`,
	Run: func(cmd *cobra.Command, args []string) {
		fmt.Println(commanders.VersionString())
	},
}

var upgrade = &cobra.Command{
	Use:   "upgrade",
	Short: "starts upgrade process",
	Long:  `starts upgrade process`,
}

var subStartHub = &cobra.Command{
	Use:   "start-hub",
	Short: "starts the hub",
	Long:  "starts the hub",
	Run: func(cmd *cobra.Command, args []string) {
		preparer := commanders.Preparer{}
		err := preparer.StartHub()
		if err != nil {
			gplog.Error(err.Error())
			os.Exit(1)
		}

		conn, connConfigErr := grpc.Dial("localhost:"+hubPort, grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}
		client := pb.NewCliToHubClient(conn)
		err = preparer.VerifyConnectivity(client)

		if err != nil {
			gplog.Error("gp_upgrade is unable to connect via gRPC to the hub")
			gplog.Error("%v", err)
			os.Exit(1)
		}
	},
}

var subShutdownClusters = &cobra.Command{
	Use:   "shutdown-clusters",
	Short: "shuts down both old and new cluster",
	Long:  "Current assumptions is both clusters exist.",
	Run: func(cmd *cobra.Command, args []string) {
		conn, connConfigErr := grpc.Dial("localhost:"+hubPort, grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}
		client := pb.NewCliToHubClient(conn)
		preparer := commanders.NewPreparer(client)
		err := preparer.ShutdownClusters(oldBinDir, newBinDir)
		if err != nil {
			gplog.Error(err.Error())
			os.Exit(1)
		}
	},
}

var subStartAgents = &cobra.Command{
	Use:   "start-agents",
	Short: "start agents on segment hosts",
	Long:  "start agents on all segments",
	Run: func(cmd *cobra.Command, args []string) {
		conn, connConfigErr := grpc.Dial("localhost:"+hubPort, grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}
		client := pb.NewCliToHubClient(conn)
		preparer := commanders.NewPreparer(client)
		err := preparer.StartAgents()
		if err != nil {
			gplog.Error(err.Error())
			os.Exit(1)
		}
	},
}

var subInitCluster = &cobra.Command{
	Use:   "init-cluster",
	Short: "inits the cluster",
	Long:  "Current assumptions is that the cluster already exists. And will only generate json config file for now.",
	Run: func(cmd *cobra.Command, args []string) {
		conn, connConfigErr := grpc.Dial("localhost:"+hubPort, grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}
		client := pb.NewCliToHubClient(conn)
		preparer := commanders.NewPreparer(client)
		err := preparer.InitCluster(newClusterDbPort)
		if err != nil {
			gplog.Error(err.Error())
			os.Exit(1)
		}
	},
}

var subUpgrade = &cobra.Command{
	Use:   "upgrade",
	Short: "the status of the upgrade",
	Long:  "the status of the upgrade",
	Run: func(cmd *cobra.Command, args []string) {
		conn, connConfigErr := grpc.Dial("localhost:"+hubPort, grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}
		client := pb.NewCliToHubClient(conn)
		reporter := commanders.NewReporter(client)
		err := reporter.OverallUpgradeStatus()
		if err != nil {
			gplog.Error(err.Error())
			os.Exit(1)
		}
	},
}

var subVersion = &cobra.Command{
	Use:     "version",
	Short:   "validate current version is upgradable",
	Long:    `validate current version is upgradable`,
	Aliases: []string{"ver"},
	RunE: func(cmd *cobra.Command, args []string) error {
		conn, connConfigErr := grpc.Dial("localhost:"+hubPort,
			grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}
		client := pb.NewCliToHubClient(conn)
		return commanders.NewVersionChecker(client).Execute(masterHost, dbPort)
	},
}

var subObjectCount = &cobra.Command{
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

var subDiskSpace = &cobra.Command{
	Use:     "disk-space",
	Short:   "check that disk space usage is less than 80% on all segments",
	Long:    "check that disk space usage is less than 80% on all segments",
	Aliases: []string{"du"},
	RunE: func(cmd *cobra.Command, args []string) error {
		conn, connConfigErr := grpc.Dial("localhost:"+hubPort,
			grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}
		client := pb.NewCliToHubClient(conn)
		return commanders.NewDiskUsageChecker(client).Execute()
	},
}

var subConversion = &cobra.Command{
	Use:   "conversion",
	Short: "the status of the conversion",
	Long:  "the status of the conversion",
	Run: func(cmd *cobra.Command, args []string) {
		conn, connConfigErr := grpc.Dial("localhost:"+hubPort, grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}
		client := pb.NewCliToHubClient(conn)
		reporter := commanders.NewReporter(client)
		err := reporter.OverallConversionStatus()
		if err != nil {
			gplog.Error(err.Error())
			os.Exit(1)
		}
	},
}

var subConfig = &cobra.Command{
	Use:   "config",
	Short: "gather cluster configuration",
	Long:  "gather cluster configuration",
	Run: func(cmd *cobra.Command, args []string) {
		conn, connConfigErr := grpc.Dial("localhost:"+hubPort,
			grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}
		client := pb.NewCliToHubClient(conn)
		err := commanders.NewConfigChecker(client).Execute(dbPort)
		if err != nil {
			gplog.Error(err.Error())
			os.Exit(1)
		}
	},
}

var subSeginstall = &cobra.Command{
	Use:   "seginstall",
	Short: "confirms that the new software is installed on all segments",
	Long: "Running this command will validate that the new software is installed on all segments, " +
		"and register successful or failed validation (available in `gp_upgrade status upgrade`)",
	Run: func(cmd *cobra.Command, args []string) {
		conn, connConfigErr := grpc.Dial("localhost:"+hubPort, grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}
		client := pb.NewCliToHubClient(conn)

		err := commanders.NewSeginstallChecker(client).Execute()
		if err != nil {
			gplog.Error(err.Error())
			os.Exit(1)
		}

		fmt.Println("Seginstall is underway. Use command \"gp_upgrade status upgrade\" " +
			"to check its current status, and/or hub logs for possible errors.")
	},
}

var subConvertMaster = &cobra.Command{
	Use:   "convert-master",
	Short: "start upgrade process on master",
	Long:  `start upgrade process on master`,
	Run: func(cmd *cobra.Command, args []string) {
		conn, connConfigErr := grpc.Dial("localhost:"+hubPort,
			grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}

		client := pb.NewCliToHubClient(conn)
		err := commanders.NewUpgrader(client).ConvertMaster(oldDataDir, oldBinDir, newDataDir, newBinDir)
		if err != nil {
			gplog.Error(err.Error())
			os.Exit(1)
		}
	},
}

var subConvertPrimaries = &cobra.Command{
	Use:   "convert-primaries",
	Short: "start upgrade process on primary segments",
	Long:  `start upgrade process on primary segments`,
	Run: func(cmd *cobra.Command, args []string) {
		conn, connConfigErr := grpc.Dial("localhost:"+hubPort,
			grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}

		client := pb.NewCliToHubClient(conn)
		err := commanders.NewUpgrader(client).ConvertPrimaries(oldBinDir, newBinDir)
		if err != nil {
			gplog.Error(err.Error())
			os.Exit(1)
		}
	},
}

var subShareOids = &cobra.Command{
	Use:   "share-oids",
	Short: "share oid files across cluster",
	Long:  `share oid files generated by pg_upgrade on master, across cluster`,
	Run: func(cmd *cobra.Command, args []string) {
		conn, connConfigErr := grpc.Dial("localhost:"+hubPort,
			grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}

		client := pb.NewCliToHubClient(conn)
		err := commanders.NewUpgrader(client).ShareOids()
		if err != nil {
			gplog.Error(err.Error())
			os.Exit(1)
		}
	},
}

var subValidateStartCluster = &cobra.Command{
	Use:   "validate-start-cluster",
	Short: "Attempt to start upgraded cluster",
	Long:  `Use gpstart in order to validate that the new cluster can successfully transition from a stopped to running state`,
	Run: func(cmd *cobra.Command, args []string) {
		conn, connConfigErr := grpc.Dial("localhost:"+hubPort,
			grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}

		client := pb.NewCliToHubClient(conn)
		err := commanders.NewUpgrader(client).ValidateStartCluster(newDataDir, newBinDir)
		if err != nil {
			gplog.Error(err.Error())
			os.Exit(1)
		}
	},
}

var subReconfigurePorts = &cobra.Command{
	Use:   "reconfigure-ports",
	Short: "Set master port on upgraded cluster to the value from the older cluster",
	Long:  `Set master port on upgraded cluster to the value from the older cluster`,
	Run: func(cmd *cobra.Command, args []string) {
		conn, connConfigErr := grpc.Dial("localhost:"+hubPort,
			grpc.WithInsecure())
		if connConfigErr != nil {
			gplog.Error(connConfigErr.Error())
			os.Exit(1)
		}

		client := pb.NewCliToHubClient(conn)
		err := commanders.NewUpgrader(client).ReconfigurePorts()
		if err != nil {
			gplog.Error(err.Error())
			os.Exit(1)
		}
	},
}
