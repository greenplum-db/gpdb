package main

import (
	"gp_upgrade/hub/cluster"
	"gp_upgrade/hub/services"
	"net"

	"google.golang.org/grpc"
	"google.golang.org/grpc/reflection"

	pb "gp_upgrade/idl"

	"fmt"
	hubLogger "gp_upgrade/hub/logger"
	"os"
	"runtime/debug"

	gpbackupUtils "github.com/greenplum-db/gp-common-go-libs/gplog"
	"github.com/spf13/cobra"
)

const (
	cliToHubPort = ":7527"
)

// This directory to have the implementation code for the gRPC server to serve
// Minimal CLI command parsing to embrace that booting this binary to run the hub might have some flags like a log dir

func main() {
	var logdir string
	var RootCmd = &cobra.Command{
		Use:   "gp_upgrade_hub [--log-directory path]",
		Short: "Start the gp_upgrade_hub (blocks)",
		Long:  `Start the gp_upgrade_hub (blocks)`,
		RunE: func(cmd *cobra.Command, args []string) error {
			debug.SetTraceback("all")
			gpbackupUtils.InitializeLogging("gp_upgrade_hub", logdir)
			errorChannel := make(chan error)
			defer close(errorChannel)
			lis, err := net.Listen("tcp", cliToHubPort)
			if err != nil {
				gpbackupUtils.Fatal(err, "failed to listen")
			}

			channelLogger := hubLogger.LogEntry{Info: make(chan string), Error: make(chan string), Done: make(chan bool)}
			server := grpc.NewServer()
			clusterPair := cluster.Pair{}
			myImpl := services.NewCliToHubListener(channelLogger, &clusterPair)
			pb.RegisterCliToHubServer(server, myImpl)
			reflection.Register(server)
			go func(myListener net.Listener) {
				if err := server.Serve(myListener); err != nil {
					gpbackupUtils.Fatal(err, "failed to serve", err)
					errorChannel <- err
				}

				close(errorChannel)
			}(lis)

			go func(channelLogger hubLogger.LogEntry) {
				for {
					select {
					case infoMsg := <-channelLogger.Info:
						gpbackupUtils.Info(infoMsg)
					case errorMsg := <-channelLogger.Error:
						fmt.Println("got error log")
						gpbackupUtils.Error(errorMsg)
					}
				}
			}(channelLogger)

			select {
			case err := <-errorChannel:
				if err != nil {
					gpbackupUtils.Fatal(err, "error during Listening")
				}
			}
			return nil
		},
	}

	RootCmd.PersistentFlags().StringVar(&logdir, "log-directory", "", "gp_upgrade_hub log directory")

	if err := RootCmd.Execute(); err != nil {
		gpbackupUtils.Error(err.Error())
		os.Exit(1)
	}

}
