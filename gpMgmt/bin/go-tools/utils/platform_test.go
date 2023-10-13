package utils_test

import (
	"bytes"
	"errors"
	"fmt"
	"os"
	"os/exec"
	"reflect"
	"strings"
	"testing"

	"github.com/greenplum-db/gpdb/gp/constants"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/idl"
	"github.com/greenplum-db/gpdb/gp/testutils/exectest"
	"github.com/greenplum-db/gpdb/gp/utils"
)

func init() {
	exectest.RegisterMains(
		ServiceStatusOutput,
		ServiceStopped,
	)
}

// Enable exectest.NewCommand mocking.
func TestMain(m *testing.M) {
	os.Exit(exectest.Run(m))
}

func setMocks(t *testing.T) {
	utils.GpsyncCommand = nil
	utils.LoadServiceCommand = nil
	utils.UnloadServiceCommand = nil
}

func resetMocks(t *testing.T) {
	utils.GpsyncCommand = exec.Command
	utils.LoadServiceCommand = exec.Command
	utils.UnloadServiceCommand = exec.Command
}

func TestCreateServiceDir(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("CreateServiceDir returns error", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		utils.SetExecCommand(exectest.NewCommand(exectest.Failure))
		defer utils.ResetExecCommand()

		err := platform.CreateServiceDir([]string{"host1"}, "path/to/serviceDir", "gphome")
		if err.Error() != "could not create service directory path/to/serviceDir on hosts: exit status 1" {
			t.Fatalf("unexpected error: %#v", err)
		}
	})

	t.Run("CreateServiceDir runs successfully", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		utils.SetExecCommand(exectest.NewCommand(exectest.Success))
		defer utils.ResetExecCommand()

		err := platform.CreateServiceDir([]string{"host1"}, "path/to/serviceDir", "gphome")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})
}

func TestWriteServiceFile(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("WriteServiceFile errors when could not open the file", func(t *testing.T) {
		file, err := os.CreateTemp("", "test")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		defer os.Remove(file.Name())

		err = os.Chmod(file.Name(), 0000)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		err = utils.WriteServiceFile(file.Name(), "abc")
		if !strings.HasPrefix(err.Error(), "could not create service file") {
			t.Fatalf("unexpected error: %#v", err)
		}
	})

	t.Run("WriteServiceFile successfully writes to a file", func(t *testing.T) {
		file, err := os.CreateTemp("", "test")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
		defer os.Remove(file.Name())

		expected := "abc"
		err = utils.WriteServiceFile(file.Name(), expected)
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		buf, err := os.ReadFile(file.Name())
		if err != nil {
			t.Fatalf("error reading file %q: %v", file.Name(), err)
		}
		contents := string(buf)

		if contents != expected {
			t.Fatalf("got %q, want %q", contents, expected)
		}
	})
}

func TestGenerateServiceFileContents(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("GenerateServiceFileContents successfully generates contents for darwin", func(t *testing.T) {
		platform := GetPlatform("darwin", t)

		expected := fmt.Sprintf(`<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>gp_hub</string>
    <key>ProgramArguments</key>
    <array>
        <string>/test/bin/gp</string>
        <string>hub</string>
    </array>
    <key>StandardOutPath</key>
    <string>/tmp/grpc_hub.log</string>
    <key>StandardErrorPath</key>
    <string>/tmp/grpc_hub.log</string>
    <key>EnvironmentVariables</key>
    <dict>
        <key>PATH</key>
        <string>%[1]s</string>
        <key>GPHOME</key>
        <string>/test</string>
    </dict>
</dict>
</plist>
`, os.Getenv("PATH"))
		contents := platform.GenerateServiceFileContents("hub", "/test", "gp")
		if contents != expected {
			t.Fatalf("got %q, want %q", contents, expected)
		}
	})

	t.Run("GenerateServiceFileContents successfully generates contents for linux", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		expected := `[Unit]
Description=Greenplum Database management utility hub

[Service]
Type=simple
Environment=GPHOME=/test
ExecStart=/test/bin/gp hub
Restart=on-failure

[Install]
Alias=gp_hub.service
WantedBy=default.target
`
		contents := platform.GenerateServiceFileContents("hub", "/test", "gp")
		if contents != expected {
			t.Fatalf("got %q, want %q", contents, expected)
		}
	})
}

func TestGetDefaultServiceDir(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("GetDefaultServiceDir returns the correct directory path", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		expected := "/home/%s/.config/systemd/user"
		result := platform.GetDefaultServiceDir()
		if result != expected {
			t.Fatalf("got %q, want %q", result, expected)
		}

		platform = GetPlatform("darwin", t)

		expected = "/Users/%s/Library/LaunchAgents"
		result = platform.GetDefaultServiceDir()
		if result != expected {
			t.Fatalf("got %q, want %q", result, expected)
		}
	})
}

func TestReloadServices(t *testing.T) {
	testhelper.SetupTestLogger()

	type test struct {
		os        string
		service   string
		errSuffix string
	}

	success_tests := []test{
		{os: "darwin", service: "hub"},
		{os: "darwin", service: "agent"},
		{os: constants.PlatformLinux, service: "hub"},
		{os: constants.PlatformLinux, service: "agent"},
	}
	for _, tc := range success_tests {
		t.Run(fmt.Sprintf("reloading of %s service succeeds on %s", tc.service, tc.os), func(t *testing.T) {
			var err error
			platform := GetPlatform(tc.os, t)

			setMocks(t)
			defer resetMocks(t)
			utils.UnloadServiceCommand = exectest.NewCommand(exectest.Success)
			utils.LoadServiceCommand = exectest.NewCommand(exectest.Success)

			utils.SetExecCommand(exectest.NewCommand(exectest.Success))
			defer utils.ResetExecCommand()

			if tc.service == "hub" {
				err = platform.ReloadHubService("/path/to/service/file")
			} else {
				err = platform.ReloadAgentService("gphome", []string{"host1"}, "/path/to/service/file")
			}

			if err != nil {
				t.Fatalf("unexpected error: %#v", err)
			}
		})
	}

	failure_tests_darwin := []test{
		{os: "darwin", service: "hub"},
		{os: "darwin", service: "agent", errSuffix: " on segment hosts"},
	}
	for _, tc := range failure_tests_darwin {
		t.Run(fmt.Sprintf("reloading of %s service returns error when not able to unload the file on darwin", tc.service), func(t *testing.T) {
			var err error
			platform := GetPlatform(tc.os, t)

			setMocks(t)
			defer resetMocks(t)
			utils.UnloadServiceCommand = exectest.NewCommand(exectest.Failure)
			utils.LoadServiceCommand = exectest.NewCommand(exectest.Success)

			if tc.service == "hub" {
				err = platform.ReloadHubService("/path/to/service/file")
			} else {
				err = platform.ReloadAgentService("gphome", []string{"host1"}, "/path/to/service/file")
			}

			expectedErr := fmt.Sprintf("could not unload %s service file /path/to/service/file%s: exit status 1", tc.service, tc.errSuffix)
			if err.Error() != expectedErr {
				t.Fatalf("got %q, want %q", err, expectedErr)
			}
		})

		t.Run(fmt.Sprintf("reloading of %s service returns error when not able to load the file on darwin", tc.service), func(t *testing.T) {
			var err error
			platform := GetPlatform(constants.PlatformDarwin, t)

			setMocks(t)
			defer resetMocks(t)
			utils.UnloadServiceCommand = exectest.NewCommand(exectest.Success)
			utils.LoadServiceCommand = exectest.NewCommand(exectest.Failure)

			if tc.service == "hub" {
				err = platform.ReloadHubService("/path/to/service/file")
			} else {
				err = platform.ReloadAgentService("gphome", []string{"host1"}, "/path/to/service/file")
			}

			expectedErr := fmt.Sprintf("could not load %s service file /path/to/service/file%s: exit status 1", tc.service, tc.errSuffix)
			if err.Error() != expectedErr {
				t.Fatalf("got %q, want %q", err, expectedErr)
			}
		})

	}

	failure_tests_linux := []test{
		{os: constants.PlatformLinux, service: "hub"},
		{os: constants.PlatformLinux, service: "agent", errSuffix: " on segment hosts"},
	}
	for _, tc := range failure_tests_linux {
		t.Run(fmt.Sprintf("reloading of %s service returns error when not able to reload the file on linux", tc.service), func(t *testing.T) {
			var err error
			platform := GetPlatform(constants.PlatformLinux, t)

			utils.SetExecCommand(exectest.NewCommand(exectest.Failure))
			defer utils.ResetExecCommand()

			if tc.service == "hub" {
				err = platform.ReloadHubService("/path/to/service/file")
			} else {
				err = platform.ReloadAgentService("gphome", []string{"host1"}, "/path/to/service/file")
			}

			expectedErr := fmt.Sprintf("could not reload %s service file /path/to/service/file%s: exit status 1", tc.service, tc.errSuffix)
			if err.Error() != expectedErr {
				t.Fatalf("got %q, want %q", err, expectedErr)
			}
		})
	}
}

func TestCreateAndInstallHubServiceFile(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("CreateAndInstallHubServiceFile runs successfully", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		utils.SetWriteServiceFileFunc(func(filename, contents string) error {
			return nil
		})
		defer utils.ResetWriteServiceFileFunc()

		utils.SetExecCommand(exectest.NewCommand(exectest.Success))
		defer utils.ResetExecCommand()

		err := platform.CreateAndInstallHubServiceFile("gphome", "testdir", "gptest")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})

	t.Run("CreateAndInstallHubServiceFile errors when not able to write to a file", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		utils.SetWriteServiceFileFunc(func(filename, contents string) error {
			return os.ErrPermission
		})
		defer utils.ResetWriteServiceFileFunc()

		utils.SetExecCommand(exectest.NewCommand(exectest.Success))
		defer utils.ResetExecCommand()

		err := platform.CreateAndInstallHubServiceFile("gphome", "testdir", "gptest")
		expectedErr := os.ErrPermission
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %q, want %q", err, expectedErr)
		}
	})

	t.Run("CreateAndInstallHubServiceFile errors when not able to reload the service", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		utils.SetWriteServiceFileFunc(func(filename, contents string) error {
			return nil
		})
		defer utils.ResetWriteServiceFileFunc()

		utils.SetExecCommand(exectest.NewCommand(exectest.Failure))
		defer utils.ResetExecCommand()

		err := platform.CreateAndInstallHubServiceFile("gphome", "testdir", "gptest")
		expectedErr := "could not reload hub service file testdir/gptest_hub.service: exit status 1"
		if err.Error() != expectedErr {
			t.Fatalf("got %q, want %q", err, expectedErr)
		}
	})
}

func TestCreateAndInstallAgentServiceFile(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("CreateAndInstallAgentServiceFile runs successfully", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		utils.SetWriteServiceFileFunc(func(filename, contents string) error {
			return nil
		})
		defer utils.ResetWriteServiceFileFunc()

		setMocks(t)
		defer resetMocks(t)
		utils.GpsyncCommand = exectest.NewCommandWithVerifier(exectest.Success, func(utility string, args ...string) {
			if !strings.HasSuffix(utility, "gpsync") {
				t.Fatalf("got %q, want gpsync", utility)
			}

			expectedArgs := []string{"-h", "host1", "-h", "host2", "./gptest_agent.service", "=:testdir/gptest_agent.service"}
			if !reflect.DeepEqual(args, expectedArgs) {
				t.Fatalf("got %+v, want %+v", args, expectedArgs)
			}
		})

		utils.SetExecCommand(exectest.NewCommand(exectest.Success))
		defer utils.ResetExecCommand()

		err := platform.CreateAndInstallAgentServiceFile([]string{"host1", "host2"}, "gphome", "testdir", "gptest")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})

	t.Run("CreateAndInstallAgentServiceFile errors when gpsync fails", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		utils.SetWriteServiceFileFunc(func(filename, contents string) error {
			return nil
		})
		defer utils.ResetWriteServiceFileFunc()

		setMocks(t)
		defer resetMocks(t)
		utils.GpsyncCommand = exectest.NewCommandWithVerifier(exectest.Failure, func(utility string, args ...string) {
			if !strings.HasSuffix(utility, "gpsync") {
				t.Fatalf("got %q, want gpsync", utility)
			}

			expectedArgs := []string{"-h", "host1", "-h", "host2", "./gptest_agent.service", "=:testdir/gptest_agent.service"}
			if !reflect.DeepEqual(args, expectedArgs) {
				t.Fatalf("got %+v, want %+v", args, expectedArgs)
			}
		})

		utils.SetExecCommand(exectest.NewCommand(exectest.Success))
		defer utils.ResetExecCommand()

		err := platform.CreateAndInstallAgentServiceFile([]string{"host1", "host2"}, "gphome", "testdir", "gptest")
		expectedErr := "could not copy agent service files to segment hosts: exit status 1"
		if err.Error() != expectedErr {
			t.Fatalf("got %q, want %q", err, expectedErr)
		}
	})

	t.Run("CreateAndInstallAgentServiceFile errors when not able to write to a file", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		utils.SetWriteServiceFileFunc(func(filename, contents string) error {
			return os.ErrPermission
		})
		defer utils.ResetWriteServiceFileFunc()

		setMocks(t)
		defer resetMocks(t)
		utils.GpsyncCommand = exectest.NewCommand(exectest.Success)

		utils.SetExecCommand(exectest.NewCommand(exectest.Success))
		defer utils.ResetExecCommand()

		err := platform.CreateAndInstallAgentServiceFile([]string{"host1", "host2"}, "gphome", "testdir", "gptest")
		expectedErr := os.ErrPermission
		if !errors.Is(err, expectedErr) {
			t.Fatalf("got %q, want %q", err, expectedErr)
		}
	})

	t.Run("CreateAndInstallAgentServiceFile errors when not able to reload the service", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		utils.SetWriteServiceFileFunc(func(filename, contents string) error {
			return nil
		})
		defer utils.ResetWriteServiceFileFunc()

		setMocks(t)
		defer resetMocks(t)
		utils.GpsyncCommand = exectest.NewCommand(exectest.Success)

		utils.SetExecCommand(exectest.NewCommand(exectest.Failure))
		defer utils.ResetExecCommand()

		err := platform.CreateAndInstallAgentServiceFile([]string{"host1", "host2"}, "gphome", "testdir", "gptest")
		expectedErr := "could not reload agent service file testdir/gptest_agent.service on segment hosts: exit status 1"
		if err.Error() != expectedErr {
			t.Fatalf("got %q, want %q", err, expectedErr)
		}
	})
}

func TestGetStartHubCommand(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("GetStartHubCommand returns the correct command for linux", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		result := platform.GetStartHubCommand("gptest").Args
		expected := []string{"systemctl", "--user", "start", "gptest_hub"}
		if !reflect.DeepEqual(result, expected) {
			t.Fatalf("got %+v, want %+v", result, expected)
		}
	})

	t.Run("GetStartHubCommand returns the correct command for darwin", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformDarwin, t)

		result := platform.GetStartHubCommand("gptest").Args
		expected := []string{"launchctl", "start", "gptest_hub"}
		if !reflect.DeepEqual(result, expected) {
			t.Fatalf("got %+v, want %+v", result, expected)
		}
	})
}

func TestGetStartAgentCommandString(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("GetStartAgentCommandString returns the correct string for linux", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		result := platform.GetStartAgentCommandString("gptest")
		expected := []string{"systemctl", "--user", "start", "gptest_agent"}
		if !reflect.DeepEqual(result, expected) {
			t.Fatalf("got %+v, want %+v", result, expected)
		}
	})

	t.Run("GetStartAgentCommandString returns the correct string for darwin", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformDarwin, t)

		result := platform.GetStartAgentCommandString("gptest")
		expected := []string{"launchctl", "", "start", "gptest_agent"}
		if !reflect.DeepEqual(result, expected) {
			t.Fatalf("got %+v, want %+v", result, expected)
		}
	})
}

func TestGetServiceStatusMessage(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("GetServiceStatusMessage successfully gets the service status for linux", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		utils.SetExecCommand(exectest.NewCommandWithVerifier(ServiceStatusOutput, func(utility string, args ...string) {
			if utility != "systemctl" {
				t.Fatalf("got %q, want systemctl", utility)
			}

			expectedArgs := []string{"--user", "show", "gptest"}
			if !reflect.DeepEqual(args, expectedArgs) {
				t.Fatalf("got %+v, want %+v", args, expectedArgs)
			}
		}))
		defer utils.ResetExecCommand()

		result, _ := platform.GetServiceStatusMessage("gptest")
		expected := "got status of the service"
		if result != expected {
			t.Fatalf("got %q, want %q", result, expected)
		}
	})

	t.Run("GetServiceStatusMessage successfully gets the service status for darwin", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformDarwin, t)

		utils.SetExecCommand(exectest.NewCommandWithVerifier(ServiceStatusOutput, func(utility string, args ...string) {
			if utility != "launchctl" {
				t.Fatalf("got %q, want systemctl", utility)
			}

			expectedArgs := []string{"list", "gptest"}
			if !reflect.DeepEqual(args, expectedArgs) {
				t.Fatalf("got %+v, want %+v", args, expectedArgs)
			}
		}))
		defer utils.ResetExecCommand()

		result, _ := platform.GetServiceStatusMessage("gptest")
		expected := "got status of the service"
		if result != expected {
			t.Fatalf("got %q, want %q", result, expected)
		}
	})

	t.Run("GetServiceStatusMessage does not throw error when service is stopped", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		utils.SetExecCommand(exectest.NewCommand(ServiceStopped))
		defer utils.ResetExecCommand()

		_, err := platform.GetServiceStatusMessage("gptest")
		if err != nil {
			t.Fatalf("unexpected err: %#v", err)
		}
	})

	t.Run("GetServiceStatusMessage errors when not able to get the service status", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		utils.SetExecCommand(exectest.NewCommand(exectest.Failure))
		defer utils.ResetExecCommand()

		output, err := platform.GetServiceStatusMessage("gptest")
		if output != "" {
			t.Fatalf("expected empty output, got %q", output)
		}

		expectedErr := "exit status 1"
		if err.Error() != expectedErr {
			t.Fatalf("got %q, want %q", err, expectedErr)
		}
	})
}

func TestParseServiceStatusMessage(t *testing.T) {
	testhelper.SetupTestLogger()

	cases := []struct {
		name     string
		os       string
		message  string
		expected *idl.ServiceStatus
	}{
		{
			name: "ParseServiceStatusMessage gets status for darwin when service is running",
			os:   constants.PlatformDarwin,
			message: `
			{
				"StandardOutPath" = "/tmp/grpc_hub.log";
				"LimitLoadToSessionType" = "Aqua";
				"StandardErrorPath" = "/tmp/grpc_hub.log";
				"Label" = "gp_hub";
				"OnDemand" = true;
				"LastExitStatus" = 0;
				"PID" = 19909;
				"Program" = "/usr/local/gpdb/bin/gp";
				"ProgramArguments" = (
					"/usr/local/gpdb/bin/gp";
					"hub";
				);
			};
			`,
			expected: &idl.ServiceStatus{Status: "running", Pid: uint32(19909)},
		},
		{
			name: "ParseServiceStatusMessage gets status for darwin when service is not running",
			os:   constants.PlatformDarwin,
			message: `
			{
				"StandardOutPath" = "/tmp/grpc_hub.log";
				"LimitLoadToSessionType" = "Aqua";
				"StandardErrorPath" = "/tmp/grpc_hub.log";
				"Label" = "gp_hub";
				"OnDemand" = true;
				"LastExitStatus" = 0;
				"Program" = "/usr/local/gpdb/bin/gp";
				"ProgramArguments" = (
					"/usr/local/gpdb/bin/gp";
					"hub";
				);
			};
			`,
			expected: &idl.ServiceStatus{Status: "not running"},
		},
		{
			name: "ParseServiceStatusMessage gets status for linux when service is running",
			os:   constants.PlatformLinux,
			message: `
			ExecMainStartTimestamp=Sun 2023-08-20 14:43:35 UTC
			ExecMainStartTimestampMonotonic=286453245
			ExecMainExitTimestampMonotonic=0
			ExecMainPID=83008
			ExecMainCode=0
			ExecMainStatus=0
			`,
			expected: &idl.ServiceStatus{Status: "running", Uptime: "Sun 2023-08-20 14:43:35 UTC", Pid: uint32(83008)},
		},
		{
			name: "ParseServiceStatusMessage gets status for linux when service is not running",
			os:   constants.PlatformLinux,
			message: `
			ExecMainStartTimestampMonotonic=286453245
			ExecMainExitTimestampMonotonic=0
			ExecMainPID=0
			ExecMainCode=0
			ExecMainStatus=0
			`,
			expected: &idl.ServiceStatus{Status: "not running", Pid: uint32(0)},
		},
	}

	for _, tc := range cases {
		t.Run(tc.name, func(t *testing.T) {
			platform := GetPlatform(constants.PlatformDarwin, t)

			result := platform.ParseServiceStatusMessage(tc.message)
			if !reflect.DeepEqual(&result, tc.expected) {
				t.Fatalf("got %+v, want %+v", &result, tc.expected)
			}
		})
	}
}

func TestDisplayServiceStatus(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("DisplayServiceStatus displays the service message", func(t *testing.T) {
		var output bytes.Buffer
		platform := GetPlatform(constants.PlatformLinux, t)
		statuses := []*idl.ServiceStatus{
			{
				Host:   "sdw1",
				Status: "running",
				Pid:    1234,
				Uptime: "5H",
			},
		}

		platform.DisplayServiceStatus(&output, "hub", statuses, true)

		expected := "hub\tsdw1\trunning\t\t1234\t5H\n"
		if output.String() != expected {
			t.Fatalf("got %q, want %q", output.String(), expected)
		}
	})

	t.Run("DisplayServiceStatus displays the service message with header", func(t *testing.T) {
		var output bytes.Buffer
		platform := GetPlatform(constants.PlatformLinux, t)
		statuses := []*idl.ServiceStatus{
			{
				Host:   "sdw1",
				Status: "running",
				Pid:    1234,
				Uptime: "5H",
			},
		}

		platform.DisplayServiceStatus(&output, "hub", statuses, false)

		expected := "ROLE\tHOST\tSTATUS\t\tPID\tUPTIME\nhub\tsdw1\trunning\t\t1234\t5H\n"
		if output.String() != expected {
			t.Fatalf("got %q, want %q", output.String(), expected)
		}
	})
}

func TestEnableUserLingering(t *testing.T) {
	testhelper.SetupTestLogger()

	t.Run("EnableUserLingering run successfully for linux", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)

		utils.SetExecCommand(exectest.NewCommand(exectest.Success))
		defer utils.ResetExecCommand()

		err := platform.EnableUserLingering([]string{"host1", "host2"}, "/path/to/gphome", "serviceUser")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})

	t.Run("EnableUserLingering runs successfully for other platforms", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformDarwin, t)

		err := platform.EnableUserLingering([]string{"host1", "host2"}, "path/to/gphome", "serviceUser")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}
	})

	t.Run("EnableUserLingering returns error on failure", func(t *testing.T) {
		platform := GetPlatform(constants.PlatformLinux, t)
		utils.SetExecCommand(exectest.NewCommand(exectest.Failure))
		defer utils.ResetExecCommand()

		err := platform.EnableUserLingering([]string{"host1", "host2"}, "path/to/gphome", "serviceUser")
		expected := "could not enable user lingering: exit status 1"
		if err.Error() != expected {
			t.Fatalf("got %q, want %q", err, expected)
		}
	})
}

func ServiceStatusOutput() {
	os.Stdout.WriteString("got status of the service")
	os.Exit(0)
}

func ServiceStopped() {
	os.Exit(3)
}

func GetPlatform(os string, t *testing.T) utils.Platform {
	t.Helper()

	platform, err := utils.NewPlatform(os)
	if err != nil {
		t.Fatalf("unexpected error: %#v", err)
	}

	return platform
}
