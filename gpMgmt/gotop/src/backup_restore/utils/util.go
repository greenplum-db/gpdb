package utils

import (
	"fmt"
	"os"
	"strings"
	"time"

	"github.com/go-errors/errors"
)

var (
	FPTimeNow  = time.Now
	FPOsGetenv = os.Getenv
)

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
