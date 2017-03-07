package common

import (
	"bufio"
	"encoding/gob"
	"fmt"
	"os"
)

func Check(msg string, e error) {
	if e != nil {
		panic(fmt.Sprintf("%s: %s\n", msg, e.Error()))
	}
}

// These commands will be the way to interact with the ssh server
type CheatSheet struct {
	Response   string
	ReturnCode []byte
}

const fileName = "/tmp/test_sshd_gp_upgrade"

func (cheatSheet CheatSheet) WriteToFile() {
	f, err := os.Create(fileName)
	Check("Failed to create file", err)
	w := bufio.NewWriter(f)
	enc := gob.NewEncoder(w)

	err = enc.Encode(cheatSheet)
	Check("Failed to encode", err)
	if err = w.Flush(); err != nil {
		panic(err)
	}
	f.Close()
}

func (cheatSheet *CheatSheet) ReadFromFile() error {
	if _, err := os.Stat(fileName); os.IsNotExist(err) {
		return err
	}

	f, err := os.Open(fileName)
	if err != nil {
		return err
	}
	defer f.Close()

	r := bufio.NewReader(f)
	dec := gob.NewDecoder(r)

	dec.Decode(&cheatSheet)

	return nil
}

func (cheatSheet *CheatSheet) RemoveFile() {
	os.Remove(fileName)
}
