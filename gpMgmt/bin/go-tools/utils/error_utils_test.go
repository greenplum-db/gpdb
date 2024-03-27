package utils_test

import (
	"errors"
	"testing"

	"google.golang.org/grpc/codes"
	"google.golang.org/grpc/status"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/utils"
)

func TestFormatGrpcError(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("correctly formats the GRPC error", func(t *testing.T) {
		grpcErr := status.Error(codes.Unknown, "error")
		result := utils.FormatGrpcError(grpcErr)

		expected := errors.New("error")
		if result.Error() != expected.Error() {
			t.Fatalf("got %v, want %v", result, expected)
		}
	})

	t.Run("returns the original error if not a GRPC error", func(t *testing.T) {
		expected := errors.New("error")
		result := utils.FormatGrpcError(expected)

		if !errors.Is(result, expected) {
			t.Fatalf("got %#v, want %#v", result, expected)
		}
	})

	t.Run("returns a nil on no error", func(t *testing.T) {
		result := utils.FormatGrpcError(nil)
		if result != nil {
			t.Fatalf("unexpected error: %#v", result)
		}
	})
}
