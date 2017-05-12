package main

import (
	. "backup_restore/backup"
	"backup_restore/utils"
)

func main() {
	defer DoTeardown()
	defer utils.RecoverFromFailure()
	DoValidation()
	DoSetup()
	DoBackup()
}
