package testUtils

import (
	"bufio"
	"encoding/gob"
	"os"
)

// These commands will be the way to interact with the ssh server
type CheatSheet struct {
	Response   string
	ReturnCode []byte
}

const CHEAT_SHEET_FILE = "/tmp/test_sshd_gp_upgrade_response.txt"

func (cheatSheet CheatSheet) WriteToFile() {
	f, err := os.Create(CHEAT_SHEET_FILE)
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
	if _, err := os.Stat(CHEAT_SHEET_FILE); os.IsNotExist(err) {
		return err
	}

	f, err := os.Open(CHEAT_SHEET_FILE)
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
	os.Remove(CHEAT_SHEET_FILE)
}
