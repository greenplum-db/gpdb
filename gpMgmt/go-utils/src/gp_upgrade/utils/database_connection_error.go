package utils

import "fmt"

type DatabaseConnectionError struct {
	Parent error
}

func (e DatabaseConnectionError) ReturnCode() int {
	return 65
}

func (e DatabaseConnectionError) Prefix() string {
	return "Database Connection Error"
}

func (e DatabaseConnectionError) Error() string {
	return fmt.Sprintf("%s: %s", e.Prefix(), e.Parent.Error())
}
