package shell_parsers

import (
	"regexp"
	"strconv"
)

var segmentPortRegexp = regexp.MustCompile(`--old-port (\d+)`)

type ShellParser interface {
	IsPgUpgradeRunning(int, string) bool
}

type RealShellParser struct{}

func (parser RealShellParser) IsPgUpgradeRunning(targetPort int, output string) bool {
	if len(output) == 0 {
		return false
	}

	targetString := strconv.Itoa(targetPort)
	segmentPorts := segmentPortRegexp.FindStringSubmatch(output)

	result := false
	for i := 0; i < len(segmentPorts); i++ {
		port := segmentPorts[i]
		if port == targetString {
			result = true
			break
		}
	}

	return result
}
