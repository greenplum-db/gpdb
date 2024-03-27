package utils_test

import (
	"errors"
	"os"
	"os/exec"
	"reflect"
	"strings"
	"testing"

	"github.com/greenplum-db/gp-common-go-libs/testhelper"
	"github.com/greenplum-db/gpdb/gp/testutils"
	"github.com/greenplum-db/gpdb/gp/testutils/exectest"
	"github.com/greenplum-db/gpdb/gp/utils"
	"github.com/greenplum-db/gpdb/gp/utils/postgres"
)

func init() {
	exectest.RegisterMains(
		CommandSuccess,
		CommandFailure,
	)
}

func TestRunExecCommand(t *testing.T) {
	testhelper.SetupTestLogger()

	cmd := &postgres.Initdb{
		PgData:   "pgdata",
		Encoding: "encoding",
		Locale:   "locale",
	}

	t.Run("succesfully runs the command", func(t *testing.T) {
		utils.System.ExecCommand = exectest.NewCommand(CommandSuccess)
		defer utils.ResetSystemFunctions()

		out, err := utils.RunExecCommand(cmd, "gpHome")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		expected := "success"
		if out.String() != expected {
			t.Fatalf("got %q, want %q", out, expected)
		}
	})

	t.Run("succesfully runs the gp sourced command", func(t *testing.T) {
		var calledUtility, calledArgs string
		utils.System.ExecCommand = exectest.NewCommandWithVerifier(CommandSuccess, func(utility string, args ...string) {
			calledUtility = utility
			calledArgs = strings.Join(args, " ")
		})
		defer utils.ResetSystemFunctions()

		out, err := utils.RunGpSourcedCommand(cmd, "gpHome")
		if err != nil {
			t.Fatalf("unexpected error: %#v", err)
		}

		expectedOut := "success"
		if out.String() != expectedOut {
			t.Fatalf("got %q, want %q", out, expectedOut)
		}

		expectedUtility := "bash"
		if calledUtility != expectedUtility {
			t.Fatalf("got %q, want %q", calledUtility, expectedUtility)
		}

		expectedArgs := "-c source gpHome/greenplum_path.sh &&"
		if !strings.HasPrefix(calledArgs, expectedArgs) {
			t.Fatalf("got %q, want prefix %q", calledArgs, expectedArgs)
		}
	})

	t.Run("when command fails to execute", func(t *testing.T) {
		utils.System.ExecCommand = exectest.NewCommand(CommandFailure)
		defer utils.ResetSystemFunctions()

		out, err := utils.RunExecCommand(cmd, "gpHome")
		if err == nil {
			t.Fatalf("expected error")
		}

		var expectedErr *exec.ExitError
		if !errors.As(err, &expectedErr) {
			t.Errorf("got %T, want %T", err, expectedErr)
		}

		expectedOut := "failure"
		if out.String() != expectedOut {
			t.Fatalf("got %v, want %v", out, expectedOut)
		}
	})
}

// CommandBuilder object for testing purpose
type testCmd struct {
	FlagA string  `flag:"--flagA"`
	FlagB int     `flag:"--flagB"`
	FlagC float64 `flag:"--flagC"`
	FlagD bool    `flag:"--flagD"`
	FlagE string
}

func (c *testCmd) BuildExecCommand(gpHome string) *exec.Cmd {
	return nil
}

// Invalid CommandBuilder object with unsupported type int8
type invalidTestCmd struct {
	FlagA string  `flag:"--flagA"`
	FlagB int     `flag:"--flagB"`
	FlagC float64 `flag:"--flagC"`
	FlagD int8    `flag:"--flagD"`
}

func (c *invalidTestCmd) BuildExecCommand(gpHome string) *exec.Cmd {
	return nil
}

func TestGenerateArgs(t *testing.T) {
	_, _, logfile := testhelper.SetupTestLogger()

	cases := []struct {
		cmd      utils.CommandBuilder
		expected []string
	}{
		{
			cmd: &testCmd{
				FlagA: "valueA",
				FlagB: 123,
				FlagC: 1.23,
				FlagD: true,
				FlagE: "valueE",
			},
			expected: []string{"--flagA", "valueA", "--flagB", "123", "--flagC", "1.230000", "--flagD"},
		},
		{
			cmd: &testCmd{
				FlagA: "valueA",
				FlagB: 123,
				FlagD: false,
			},
			expected: []string{"--flagA", "valueA", "--flagB", "123"},
		},
		{
			cmd: &testCmd{
				FlagB: 123,
			},
			expected: []string{"--flagB", "123"},
		},
	}

	for _, tc := range cases {
		t.Run("succesfully generates the arguments for a command", func(t *testing.T) {
			result := utils.GenerateArgs(tc.cmd)

			if !reflect.DeepEqual(result, tc.expected) {
				t.Fatalf("got %+v, want %+v", result, tc.expected)
			}
		})
	}

	t.Run("logs error when an unsupported data type is found", func(t *testing.T) {
		cmd := &invalidTestCmd{
			FlagA: "valueA",
			FlagD: int8(1),
		}
		utils.GenerateArgs(cmd)

		testutils.AssertLogMessage(t, logfile, `\[ERROR\]:-unsupported data type int8 while generating command arguments for invalidTestCmd`)
	})
}

func CommandSuccess() {
	os.Stdout.WriteString("success")
	os.Exit(0)
}

func CommandFailure() {
	os.Stderr.WriteString("failure")
	os.Exit(1)
}
