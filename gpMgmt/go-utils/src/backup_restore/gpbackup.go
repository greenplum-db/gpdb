package main

import (
	. "backup_restore/backup"
)

func main() {
	defer DoTeardown()
	DoInit()
	DoValidation()
	DoSetup()
	DoBackup()
}
