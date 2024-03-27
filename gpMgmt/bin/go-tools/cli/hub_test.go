package cli_test

import (
	"errors"
	"fmt"
	"os"
	"reflect"
	"sync"
	"testing"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/cli"
	"github.com/greenplum-db/gpdb/gp/constants"
	"github.com/greenplum-db/gpdb/gp/testutils"
	"github.com/greenplum-db/gpdb/gp/testutils/exectest"
	"github.com/greenplum-db/gpdb/gp/utils"
)

func init() {
	exectest.RegisterMains(ValidGpsshOpt, InValidGpsshOpt, InValidGpsshOpt2, InValidGpsshOpt3)
}

func ValidGpsshOpt() {
	os.Stdout.WriteString("test output\nTEST 1234")
}

func InValidGpsshOpt() {
	os.Stdout.WriteString("test output")
}
func InValidGpsshOpt2() {
	os.Stdout.WriteString("test output\nTEST")
}
func InValidGpsshOpt3() {
	os.Stdout.WriteString("test output\nTEST TEST")
}

func TestGetUlimitSshFn(t *testing.T) {
	_, _, logile := testhelper.SetupTestLogger()
	t.Run("logs error when gpssh command execution fails", func(t *testing.T) {
		testStr := "error executing command to fetch open files limit on host"
		defer utils.ResetSystemFunctions()
		utils.System.ExecCommand = exectest.NewCommand(exectest.Failure)
		var wg sync.WaitGroup
		wg.Add(1)
		channel := make(chan cli.Response)
		cli.GetUlimitSshFn("sdw1", channel, &wg)
		testutils.AssertLogMessage(t, logile, testStr)
	})
	t.Run("logs error when gpssh output has fewer lines", func(t *testing.T) {
		testStr := "unexpected output when fetching open files limit on host"
		defer utils.ResetSystemFunctions()
		utils.System.ExecCommand = exectest.NewCommand(InValidGpsshOpt)
		var wg sync.WaitGroup
		wg.Add(1)
		channel := make(chan cli.Response)
		cli.GetUlimitSshFn("sdw1", channel, &wg)
		testutils.AssertLogMessage(t, logile, testStr)
	})
	t.Run("logs error when gpssh output has unexpected format", func(t *testing.T) {
		testStr := "unexpected output when parsing open files limit output for host"
		defer utils.ResetSystemFunctions()
		utils.System.ExecCommand = exectest.NewCommand(InValidGpsshOpt2)
		var wg sync.WaitGroup
		wg.Add(1)
		channel := make(chan cli.Response)
		cli.GetUlimitSshFn("sdw1", channel, &wg)
		testutils.AssertLogMessage(t, logile, testStr)
	})
	t.Run("logs error when gpssh output fails to convert to integer", func(t *testing.T) {
		testStr := "unexpected output when converting open files limit value for host"
		defer utils.ResetSystemFunctions()
		utils.System.ExecCommand = exectest.NewCommand(InValidGpsshOpt3)
		var wg sync.WaitGroup
		wg.Add(1)
		channel := make(chan cli.Response)
		cli.GetUlimitSshFn("sdw1", channel, &wg)
		testutils.AssertLogMessage(t, logile, testStr)
	})
	t.Run("logs error when gpssh output fails to convert to integer", func(t *testing.T) {
		defer utils.ResetSystemFunctions()
		utils.System.ExecCommand = exectest.NewCommand(ValidGpsshOpt)
		var wg sync.WaitGroup
		wg.Add(1)
		channel := make(chan cli.Response)
		go cli.GetUlimitSshFn("sdw1", channel, &wg)
		go func() {
			wg.Wait()
			close(channel)
		}()

		for result := range channel {
			if result.Ulimit != 1234 {
				t.Fatalf("got ulimit:%d, expected:1234", result.Ulimit)
			}
		}
	})
}
func TestCheckOpenFilesLimitOnHosts(t *testing.T) {
	_, _, logile := testhelper.SetupTestLogger()
	t.Run("prints warning if fails to execute Ulimit command", func(t *testing.T) {
		testStr := "test error string"
		defer utils.ResetSystemFunctions()
		utils.ExecuteAndGetUlimit = func() (int, error) {
			return -1, fmt.Errorf(testStr)
		}
		cli.CheckOpenFilesLimitOnHosts(nil)
		testutils.AssertLogMessage(t, logile, testStr)
	})
	t.Run("prints warning  Ulimit is lower than required on coordinator", func(t *testing.T) {
		testStr := "For proper functioning make sure limit is set properly for system and services before starting gp services."
		defer utils.ResetSystemFunctions()
		utils.ExecuteAndGetUlimit = func() (int, error) {
			return constants.OsOpenFiles - 1, nil
		}
		cli.CheckOpenFilesLimitOnHosts(nil)
		testutils.AssertLogMessage(t, logile, testStr)
	})
	t.Run("prints warning  Ulimit is lower than required on remote host", func(t *testing.T) {
		testStr := "For proper functioning make sure limit is set properly for system and services before starting gp services."
		defer utils.ResetSystemFunctions()
		defer func() { cli.GetUlimitSsh = cli.GetUlimitSshFn }()
		utils.ExecuteAndGetUlimit = func() (int, error) {
			return constants.OsOpenFiles + 1, nil
		}
		cli.GetUlimitSsh = func(hostname string, channel chan cli.Response, wg *sync.WaitGroup) {
			defer wg.Done()
			channel <- cli.Response{Hostname: "localhost", Ulimit: constants.OsOpenFiles - 1}
		}
		cli.CheckOpenFilesLimitOnHosts([]string{"localhost"})
		testutils.AssertLogMessage(t, logile, testStr)
	})
}
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
