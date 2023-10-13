package cli_test

import (
	"errors"
	"os"
	"reflect"
	"testing"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/cli"
)

func TestGetHostnames(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("is able to parse the hostnames correctly", func(t *testing.T) {
		file, err := os.CreateTemp("", "test")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		defer os.Remove(file.Name())

		_, err = file.WriteString("sdw1\nsdw2\nsdw3\n")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		result, err := cli.GetHostnames(file.Name())
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		expected := []string{"sdw1", "sdw2", "sdw3"}
		if !reflect.DeepEqual(result, expected) {
			t.Fatalf("got %+v, want %+v", result, expected)
		}
	})

	t.Run("errors out when not able to read from the file", func(t *testing.T) {
		file, err := os.CreateTemp("", "test")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		defer os.Remove(file.Name())

		err = os.Chmod(file.Name(), 0000)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		_, err = cli.GetHostnames(file.Name())
		if !errors.Is(err, os.ErrPermission) {
			t.Fatalf("got %v, want %v", err, os.ErrPermission)
		}
	})
}
