package commanders

import (
	"context"
	pb "gp_upgrade/idl"
)

// SeginstallChecker is a CLI command that starts the segment installation.
type SeginstallChecker struct {
	client pb.CliToHubClient
}

// NewSeginstallChecker creates and returns a new SeginstallChecker.
func NewSeginstallChecker(client pb.CliToHubClient) SeginstallChecker {
	return SeginstallChecker{
		client: client,
	}
}

// Execute makes a CheckSeginstall gRPC request to the configured Hub.
func (req SeginstallChecker) Execute() error {
	_, err := req.client.CheckSeginstall(
		context.Background(),
		&pb.CheckSeginstallRequest{},
	)
	return err
}
