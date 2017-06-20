package config

import (
	"encoding/json"
	"io/ioutil"
)

type Reader struct {
	config SegmentConfiguration
}

func (reader *Reader) Read() error {
	contents, err := ioutil.ReadFile(GetConfigFilePath())
	if err != nil {
		return err
	}
	err = json.Unmarshal([]byte(contents), &reader.config)
	return err
}

// returns -1 for not found
func (reader Reader) GetPortForSegment(segmentDbid int) int {
	var result int = -1
	for i := 0; i < len(reader.config); i++ {
		segment := reader.config[i]
		if segment.DBID == segmentDbid {
			result = segment.Port
			break
		}
	}

	return result
}
