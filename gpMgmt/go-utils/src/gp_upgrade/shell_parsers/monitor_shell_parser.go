package shell_parsers

import (
	"regexp"
	"strconv"
)

type ShellParser struct {
	Output string
}

var segmentPortRegexp = regexp.MustCompile(`--old-port (\d+)`)

func NewShellParser(output string) *ShellParser {
	return &ShellParser{Output: output}
}

func (parser ShellParser) IsPgUpgradeRunning(targetPort int) bool {
	if len(parser.Output) == 0 {
		return false
	}

	targetString := strconv.Itoa(targetPort)
	segmentPorts := segmentPortRegexp.FindStringSubmatch(parser.Output)

	var result bool = false
	for i := 0; i < len(segmentPorts); i++ {
		port := segmentPorts[i]
		if port == targetString {
			result = true
			break
		}
	}

	return result
}
