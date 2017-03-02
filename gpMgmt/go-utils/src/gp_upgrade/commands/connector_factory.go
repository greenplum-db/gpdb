package commands

import (
	"errors"
	"fmt"
)

var Connectors = make(map[string]Connector)

//func RegisterConnector(key string, connector Connector) {
//	Connectors[key] = connector
//	fmt.Printf("added connector with key: %s\n", key)
//}

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
