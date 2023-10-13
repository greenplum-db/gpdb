// Copyright (c) 2017-2023 VMware, Inc. or its affiliates
// SPDX-License-Identifier: Apache-2.0

package exectest

import "os"

// These are built-in Main implementations that seem to be common/useful to many
// tests.

// Success is a Main implementation that exits with a zero exit code.
func Success() {}

// Failure is a Main implementation that exits with an exit code of 1.
func Failure() { os.Exit(1) }

func init() {
	RegisterMains(
		Success,
		Failure,
	)
}
