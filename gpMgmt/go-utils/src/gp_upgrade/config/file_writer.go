package config

import "os"

type FileWriter interface {
	Write(*os.File, []byte) error
}
