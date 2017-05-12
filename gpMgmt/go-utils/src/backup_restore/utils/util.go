package utils

/*
 * This file contains miscellaneous functions that are generally useful and
 * don't fit into any other file.
 */

import (
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/go-errors/errors"
)

var (
	DumpTimestamp string
	DumpDatestamp string

	FPTimeNow  = time.Now
	FPOsGetenv = os.Getenv
)

/*
 * The Abort() and AbortWithStackTrace() functions are for handling critical
 * errors.  They panic() to unwind the call stack until the panic is caught
 * by the recover() in DoTeardown() in backup.go, at which point any necessary
 * cleanup is performed.
 *
 * The log.Fatal() function calls AbortWithStackTrace() if the log level is set
 * to Verbose or Debug or calls Abort() otherwise, so generally that function
 * should be used instead of calling either of these functions directly.
 */

func Abort(output ...interface{}) {
	errStr := ""
	if len(output) > 0 {
		errStr = fmt.Sprintf("%v", output[0])
		if len(output) > 1 {
			errStr = fmt.Sprintf(errStr, output[1:]...)
		}
	}
	panic(errStr)
}

func AbortWithStackTrace(output ...interface{}) {
	errStr := ""
	if len(output) > 0 {
		errStr = fmt.Sprintf("%v\n", output[0])
		if len(output) > 1 {
			errStr = fmt.Sprintf(errStr, output[1:]...)
		}
	}
	trace := string(errors.Wrap(errStr, 2).Stack()) // skip = 2 doesn't print trace for this function or logger.Fatal() call
	stacktrace := strings.Split(trace, "\n")
	trace = strings.Join(stacktrace[:len(stacktrace)-5], "\n") // Cut off the last two lines about "main: fn()" and "goexit: BYTE"

	errStr += trace
	panic(errStr)
}

func CheckError(err error) {
	if err != nil {
		logger.Fatal(err.Error())
	}
}

/*
 * General helper functions
 */

func CurrentDatestamp() string {
	return FPTimeNow().Format("20060102")
}

func CurrentTimestamp() string {
	return FPTimeNow().Format("20060102150405")
}

func TryEnv(varname string, defval string) string {
	val := FPOsGetenv(varname)
	if val == "" {
		return defval
	}
	return val
}

func SetDumpTimestamp() string {
	DumpTimestamp = CurrentTimestamp()
	DumpDatestamp = DumpTimestamp[0:8]
	return DumpTimestamp
}
