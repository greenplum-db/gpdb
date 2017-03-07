package commands

import (
	"errors"
	"fmt"
)

var Connectors = make(map[string]Connector)

func GetConnector(key string) (Connector, error) {
	result := Connectors[key]
	if result == nil {
		if key == "ssh" {
			Connectors["ssh"] = NewSshConnector()
			result = Connectors["ssh"]
		} else {
			return nil, errors.New(fmt.Sprintf("no connector of type '%s'", key))
		}
	}
	return result, nil
}
