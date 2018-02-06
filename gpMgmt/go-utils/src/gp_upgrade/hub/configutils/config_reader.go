package configutils

import (
	"encoding/json"
	"gp_upgrade/utils"

	"github.com/pkg/errors"
)

type Reader struct {
	config       SegmentConfiguration
	fileLocation string
}

func NewReader() Reader {
	return Reader{}
}

func (reader *Reader) OfOldClusterConfig() {
	reader.fileLocation = GetConfigFilePath()
}

func (reader *Reader) OfNewClusterConfig() {
	reader.fileLocation = GetNewClusterConfigFilePath()
}

func (reader *Reader) Read() error {
	if reader.fileLocation == "" {
		return errors.New("Reader file location unknown")
	}

	contents, err := utils.System.ReadFile(reader.fileLocation)

	if err != nil {
		return errors.New(err.Error())
	}
	err = json.Unmarshal([]byte(contents), &reader.config)
	if err != nil {
		return errors.New(err.Error())
	}
	return nil
}

// returns -1 for not found
func (reader Reader) GetPortForSegment(segmentDbid int) int {
	result := -1
	for i := 0; i < len(reader.config); i++ {
		segment := reader.config[i]
		if segment.DBID == segmentDbid {
			result = segment.Port
			break
		}
	}

	return result
}

func (reader Reader) GetHostnames() ([]string, error) {
	if len(reader.config) == 0 {
		err := reader.Read()
		if err != nil {
			return nil, err
		}
	}
	hostnamesSeen := make(map[string]bool)
	for i := 0; i < len(reader.config); i++ {
		_, contained := hostnamesSeen[reader.config[i].Hostname]
		if !contained {
			hostnamesSeen[reader.config[i].Hostname] = true
		}
	}
	var hostnames []string
	for k := range hostnamesSeen {
		hostnames = append(hostnames, k)
	}
	return hostnames, nil
}

func (reader Reader) GetSegmentConfiguration() SegmentConfiguration {
	return reader.config
}

func (reader Reader) GetMasterDataDir() string {
	config := reader.GetSegmentConfiguration()
	for i := 0; i < len(config); i++ {
		segment := config[i]
		if segment.Content == -1 {
			return segment.Datadir
		}
	}
	return ""
}

func (reader Reader) GetMaster() *Segment {
	var nilSegment *Segment
	config := reader.GetSegmentConfiguration()
	for i := 0; i < len(config); i++ {
		segment := config[i]
		if segment.Content == -1 {
			return &segment
		}
	}
	return nilSegment
}
