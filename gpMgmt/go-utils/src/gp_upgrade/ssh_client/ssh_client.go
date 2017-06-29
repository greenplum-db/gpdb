package ssh_client

type SshClient interface {
	NewSession() (SshSession, error)
}
