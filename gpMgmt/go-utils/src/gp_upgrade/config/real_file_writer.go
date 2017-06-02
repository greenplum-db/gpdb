package config

import "os"

type RealFileWriter struct {
}

func NewRealFileWriter() FileWriter {
	return &RealFileWriter{}
}

func (fileWriter RealFileWriter) Write(f *os.File, data []byte) error {
	_, err := f.Write(data)
	return err
}
