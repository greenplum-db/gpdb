// Copyright (c) 2017-2023 VMware, Inc. or its affiliates
// SPDX-License-Identifier: Apache-2.0

// Package exectest provides helpers for test code that wants to mock out pieces
// of the os/exec package, namely exec.Command(). For explanations see:
// https://web.archive.org/web/20220506055022/https://npf.io/2015/06/testing-exec-command/
// https://web.archive.org/web/20220506055117/https://jamiethompson.me/posts/Unit-Testing-Exec-Command-In-Golang/
package exectest

import (
	"fmt"
	"os"
	"os/exec"
	"reflect"
	"strconv"
	"strings"
	"testing"
)

// Main represents the main function of an executable.
type Main func()

// Command is a function that has an identical signature to exec.Command(). It
// is created with a call to NewCommand().
type Command func(string, ...string) *exec.Cmd

// mains is populated by RegisterMains and used as a lookup table by Run and
// NewCommand.
var mains []Main

// magicString is the signal, passed inside os.Args[0], that Run() should invoke
// a command and exit. It is expected to be followed by the integer index of the
// Main function in mains that should be invoked.
const magicString = "EXECTEST_MAIN_INDEX"

// runCalled tracks whether or not Run() has been called by the test package.
// It's a quick-and-dirty failsafe against developers forgetting to use the
// handler (which would cause infinite recursion).
var runCalled = false

// NewCommand returns a drop-in replacement for os/exec.Command. The exec.Cmd
// that the Command returns will invoke the passed Main function in a new test
// subprocess. The original arguments passed to the returned Command will be
// passed to the subprocess.
//
// The intended use case is for test packages to replace exec.Command in the
// packages they are testing with the resulting Command created by this
// function.
//
// To use NewCommand, two pieces of test boilerplate are required. The first is
// that any Main functions must be explicitly declared to the exectest package
// by calling RegisterMains from the test package's init function:
//
//	func MyExecutable() {
//	    os.Exit(1)
//	}
//
//	func init() {
//	    exectest.RegisterMains(
//	        MyExecutable,
//	    )
//	}
//
// The second is that the test suite must be started using exectest.Run:
//
//	func TestMain(m *testing.M) {
//	    os.Exit(exectest.Run(m))
//	}
//
// Once these conditions are met, the function handed back from NewCommand may
// be used exactly like you would use exec.Command():
//
//	execCommand := exectest.NewCommand(MyExecutable)
//
//	cmd := execCommand("/bin/bash", "-c", "sleep 15")
//	cmd.Run() // this invokes MyExecutable, not Bash
func NewCommand(m Main) Command {
	// Sanity check. Ensure that our two boilerplate conditions have been met.
	index := indexOf(m, mains)
	if index < 0 {
		// m is not in mains.
		panic("Main functions must be registered using RegisterMains() in init()")
	}
	if !runCalled {
		panic("test packages using NewCommand must invoke Run from TestMain()")
	}

	return func(executable string, args ...string) *exec.Cmd {
		// Pass the original arguments to the process.
		cmd := exec.Command(os.Args[0], args...)

		// Hijack argv[0] to communicate to Run() that the invoked test process
		// should execute a Main function and then exit. The original executable
		// name is also passed here so that the added magic can be stripped back
		// off on the other side.
		cmd.Args[0] = fmt.Sprintf("%s=%d=%s", magicString, index, executable)
		return cmd
	}
}

// NewCommandWithVerifier works like NewCommand, with an additional verifier
// callback that is run against the supplied arguments. This allows unit tests
// to explicitly check the arguments that are passed to exec.Command():
//
//	execCommand := exectest.NewCommandWithVerifier(MyExecutable,
//	    func(name string, arg ...string)) {
//	        if name != "/bin/bash" {
//	            t.Errorf("didn't use Bash")
//	        }
//	    }
//	)
//
//	execCommand("/bin/bash", "-c", "sleep 1") // succeeds
//	execCommand("/bin/sh", "-c", "sleep 1")   // logs a test failure
func NewCommandWithVerifier(m Main, verifier func(string, ...string)) Command {
	cmdf := NewCommand(m)

	return func(executable string, args ...string) *exec.Cmd {
		// Run the verifier, then invoke the underlying Command.
		verifier(executable, args...)
		return cmdf(executable, args...)
	}
}

// RegisterMains makes multiple Main functions available to Run in the test
// subprocess. Call it only from your test package's init functions. Any Main
// functions passed to NewCommand must be registered using this function.
func RegisterMains(m ...Main) {
	if runCalled {
		// Try to catch the most obvious failure mode, where a developer
		// accidentally calls this from a test.
		panic("RegisterMains() must be called only from a test package's init function")
	}
	mains = append(mains, m...)
}

// Run is a wrapper for testing.M.Run() to use in conjunction with
// RegisterMains() and NewCommand().
//
// During the first run of the test executable, it simply calls m.Run() and
// returns its exit code. When the test executable is reinvoked by a Command
// implementation, it uses the information contained in os.Args[0] to execute a
// Main function instead. In that case, Run will not return.
func Run(m *testing.M) int {
	// It's safe for code to call NewCommand now.
	runCalled = true

	name := os.Args[0]
	if !strings.HasPrefix(name, magicString) {
		// Allow the test suite to continue.
		return m.Run()
	}

	// The format is <magic>=<index>=<original executable>, e.g.
	//     EXECTEST_MAIN_INDEX=3=/usr/local/bin/myexec
	components := strings.SplitN(name, "=", 3)
	index := components[1]
	os.Args[0] = components[2]

	// Look up the desired Main function.
	i, err := strconv.Atoi(index)
	if err != nil || i < 0 {
		panic(fmt.Sprintf("received invalid index %#v from %s", index, name))
	} else if i >= len(mains) {
		panic("Main functions must be registered using RegisterMains() in init()")
	}

	// Invoke. This may or may not return.
	mains[i]()

	// The invoked Main might exit itself, but if not we should exit here
	// instead of returning control to the test suite.
	os.Exit(0)
	panic("unreachable")
}

func indexOf(m Main, list []Main) int {
	// XXX Function pointers are not directly comparable in Go. We hack around
	// this using reflection, but this is not guaranteed to work the same way
	// in the future...
	this := reflect.ValueOf(m).Pointer()

	for i, candidate := range list {
		that := reflect.ValueOf(candidate).Pointer()
		if this == that {
			return i
		}
	}
	return -1 // not found
}
