package utils

import (
	"fmt"

	"google.golang.org/grpc/status"

	"github.com/greenplum-db/gp-common-go-libs/gplog"
)

func FormatGrpcError(err error) error {
	if err == nil {
		return nil
	}

	grpcErr, ok := status.FromError(err)
	if ok {
		errorDescription := grpcErr.Message()
		return fmt.Errorf(errorDescription)
	}

	return err
}

/*
LogAndReturnError logs the error using gplog and returns error.
Make sure to use in hub and agent only
*/
func LogAndReturnError(err error) error {
	gplog.Error(err.Error())
	return err
}
