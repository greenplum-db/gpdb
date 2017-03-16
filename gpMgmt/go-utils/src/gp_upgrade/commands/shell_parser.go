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
	var segmentPortRegexp = regexp.MustCompile(`--old-port (\d+)`)
	segmentPorts := segmentPortRegexp.FindStringSubmatch(parser.Output)

	//TODO: "We'd like to know if %v has pg_upgrade running for it, but not yet implemented", parser.Segment_id

	return segmentPorts != nil
}
