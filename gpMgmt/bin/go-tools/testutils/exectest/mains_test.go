// Copyright (c) 2017-2023 VMware, Inc. or its affiliates
// SPDX-License-Identifier: Apache-2.0

package exectest_test

import (
	"errors"
	"os/exec"
	"testing"

	. "github.com/greenplum-db/gpdb/gp/testutils/exectest"
)

func TestBuiltinMains(t *testing.T) {
	t.Run("Success()", func(t *testing.T) {
		success := NewCommand(Success)("/unused/path")
		err := success.Run()
		if err != nil {
			t.Errorf("exited with error %v", err)

			var exitErr *exec.ExitError
			if errors.As(err, &exitErr) {
				t.Logf("subprocess stderr follows:\n%s", string(exitErr.Stderr))
			}
		}
	})

	t.Run("Failure()", func(t *testing.T) {
		failure := NewCommand(Failure)("/unused/path")
		err := failure.Run()
		if err == nil {
			t.Fatal("exited without an error")
		}

		var exitErr *exec.ExitError
		if !errors.As(err, &exitErr) {
			t.Fatalf("got error %#v, want type %T", err, exitErr)
		}
		if exitErr.ExitCode() != 1 {
			t.Errorf("exit code is %d, want %d", exitErr.ExitCode(), 1)
		}
	})
}
