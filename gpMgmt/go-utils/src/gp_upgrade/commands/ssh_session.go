package commands

type SshSession struct {
}

func (sshSession SshSession) isPg_UpgradeRunning(segment_id string) bool {
	return false
}
