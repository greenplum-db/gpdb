// Copyright (c) 2017-2023 VMware, Inc. or its affiliates
// SPDX-License-Identifier: Apache-2.0

package exectest

import (
	"fmt"
	"os"
	"os/exec"
	"reflect"
	"testing"
)

const successfulStdout = "stdout for SuccessfulMain"

func SuccessfulMain() {
	fmt.Print(successfulStdout)
}

const failedStderr = "stderr for FailedMain"
const failedCode = 1

func FailedMain() {
	fmt.Fprint(os.Stderr, failedStderr)
	os.Exit(failedCode)
}

var expectedCheckedArgs = []string{"arg1", "arg2", "arg3"}

// ArgumentCheckingMain expects to be executed with the expectedCheckedArgs
// above. If not it will print the difference and exit with an error.
func ArgumentCheckingMain() {
	if !reflect.DeepEqual(os.Args, expectedCheckedArgs) {
		fmt.Fprintf(os.Stderr, "got args %#v want %#v", os.Args, expectedCheckedArgs)
		os.Exit(1)
	}
}

func UnregisteredMain() {}

// EnvironmentMain prints out its entire environment, one per line, in
// NAME=VALUE format.
func EnvironmentMain() {
	for _, e := range os.Environ() {
		fmt.Println(e)
	}
}

func init() {
	RegisterMains(
		SuccessfulMain,
		FailedMain,
		// UnregisteredMain is intentionally missing
	)
}

// Ensure that multiple calls to RegisterMains() from multiple init() functions
// work as expected.
func init() {
	RegisterMains(
		ArgumentCheckingMain,
		EnvironmentMain,
		// UnregisteredMain is intentionally missing
	)
}

func TestMain(m *testing.M) {
	os.Exit(Run(m))
}

func TestNewCommand(t *testing.T) {
	t.Run("panics if Main isn't registered", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Errorf("did not panic")
			}
		}()

		NewCommand(UnregisteredMain)
	})

	t.Run("panics if not called from Run()", func(t *testing.T) {
		// We're obviously being called from Run() inside this test, so fake the
		// situation by unsetting the flag that tracks it.
		runCalled = false

		defer func() {
			if r := recover(); r == nil {
				t.Errorf("did not panic")
			}
			runCalled = true // reset the flag
		}()

		NewCommand(SuccessfulMain)
	})

	t.Run("invokes passed Main", func(t *testing.T) {
		// SuccessfulMain prints to stdout and exits with code zero.
		cmd := NewCommand(SuccessfulMain)("/unused/path")

		outb, err := cmd.Output()
		out := string(outb)

		if err != nil {
			t.Errorf("Output() returned error: %v", err)
			if exitErr, ok := err.(*exec.ExitError); ok {
				t.Errorf("subprocess stderr follows:\n%s", string(exitErr.Stderr))
			}
		}
		if out != successfulStdout {
			t.Errorf("Output() = %#v want %#v", out, successfulStdout)
		}
	})

	t.Run("handles common error conditions", func(t *testing.T) {
		// SuccessfulMain prints to stderr and exits with code 1.
		cmd := NewCommand(FailedMain)("/unused/path")

		outb, err := cmd.Output()
		out := string(outb)

		if out != "" {
			t.Errorf("unexpected output %#v", out)
		}

		exitErr, ok := err.(*exec.ExitError)
		if !ok {
			t.Fatalf("unexpected error %#v", err)
		}
		if exitErr.ExitCode() != failedCode {
			t.Errorf("exit code %d want %d", exitErr.ExitCode(), failedCode)
		}

		stderr := string(exitErr.Stderr)
		if stderr != failedStderr {
			t.Errorf("stderr %#v want %#v", stderr, failedStderr)
		}
	})

	t.Run("passes arguments to its Main process", func(t *testing.T) {
		cmdFunc := NewCommand(ArgumentCheckingMain)
		cmd := cmdFunc(expectedCheckedArgs[0], expectedCheckedArgs[1:]...)

		_, err := cmd.Output()
		if err != nil {
			t.Errorf("Output() returned error: %v", err)
			if exitErr, ok := err.(*exec.ExitError); ok {
				t.Errorf("subprocess stderr follows:\n%s", string(exitErr.Stderr))
			}
		}
	})

	t.Run("still allows Cmd.Env to function as expected", func(t *testing.T) {
		cmd := NewCommand(EnvironmentMain)("/unused/path")

		cmd.Env = []string{"A=1", "B=2"}

		out, err := cmd.Output()
		if err != nil {
			t.Errorf("Output() returned error: %v", err)
			if exitErr, ok := err.(*exec.ExitError); ok {
				t.Errorf("subprocess stderr follows:\n%s", string(exitErr.Stderr))
			}
		}

		actual := string(out)
		expected := "A=1\nB=2\n"
		if actual != expected {
			t.Errorf("Output() = %#v want %#v", actual, expected)
		}
	})
}

func TestNewCommandWithVerifier(t *testing.T) {
	t.Run("panics immediately on failure", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Errorf("did not panic")
			}
		}()

		NewCommandWithVerifier(UnregisteredMain, func(string, ...string) {})
	})

	t.Run("runs verifier when Command is called", func(t *testing.T) {
		executable := "/bin/echo"
		args := []string{"hello", "there"}
		called := false

		v := func(e string, a ...string) {
			called = true

			if e != executable {
				t.Errorf("executable = %#v want %#v", e, executable)
			}
			if !reflect.DeepEqual(a, args) {
				t.Errorf("args = %#v want %#v", a, args)
			}
		}

		cmdFunc := NewCommandWithVerifier(SuccessfulMain, v)

		if called {
			t.Errorf("verifier called prematurely")
		}

		cmdFunc(executable, args...)

		if !called {
			t.Errorf("verifier was not called")
		}
	})
}

func TestRegisterMains(t *testing.T) {
	t.Run("panics if called from Run", func(t *testing.T) {
		defer func() {
			if r := recover(); r == nil {
				t.Errorf("did not panic")
			}
		}()

		RegisterMains()
	})
}
