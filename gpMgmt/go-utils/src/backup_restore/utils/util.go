package utils

import (
	"fmt"
	"os"
	"time"
)

var (
	FPTimeNow  = time.Now
	FPOsGetenv = os.Getenv
)

// Pass in printf()-style message and interpolation args, then end the command appropriately
func Abort(output ...interface{}) {
	errStr := ""
	if len(output) > 0 {
		errStr = fmt.Sprintf("%v\n", output[0])
		if len(output) > 1 {
			errStr = fmt.Sprintf(errStr, output[1:]...)
		}
	}
	panic(errStr)
}

func CheckError(err error) {
	if err != nil {
		Abort(err)
	}
}

func CurrentTimestamp() string {
	return FPTimeNow().Format("20060102150405")
}

func RecoverFromFailure() {
	if r := recover(); r != nil {
		fmt.Printf("[CRITICAL] %v\n", r) // TODO: Replace with logging command when we implement that
	}
}

func TryEnv(varname string, defval string) string {
	val := FPOsGetenv(varname)
	if val == "" {
		return defval
	}
	return val
}
