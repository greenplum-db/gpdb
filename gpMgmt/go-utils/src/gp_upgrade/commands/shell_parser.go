package commands

import (
	"regexp"
)

type ShellParser struct {
	Output string
}

func NewShellParser(output string) *ShellParser {
	return &ShellParser{Output: output}
}

func (parser ShellParser) IsPgUpgradeRunning() bool {
	if len(parser.Output) == 0 {
		return false
	}
	// todo can this regexp be compiled statically?
	var segmentPortRegexp = regexp.MustCompile(`--old-port (\d+)`)
	segmentPorts := segmentPortRegexp.FindStringSubmatch(parser.Output)

	return segmentPorts != nil
}
