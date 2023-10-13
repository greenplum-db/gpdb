package utils_test

import (
	"os"
	"os/exec"
	"testing"

	"github.com/greenplum-db/gpdb/gp/constants"

	"github.com/greenplum-db/gpdb/gp/utils"
)

func TestLoadServerCredentials(t *testing.T) {
	err := exec.Command(constants.ShellPath, "-c", "../generate_test_tls_certificates.sh `hostname`").Run()
	if err != nil {
		t.Fatalf("Cannot generate test certificates: %v", err)
	}

	t.Run("successfully parses good certificate files", func(t *testing.T) {
		creds := &utils.GpCredentials{
			CACertPath:     "./certificates/ca-cert.pem",
			CAKeyPath:      "./certificates/ca-key.pem",
			ServerCertPath: "./certificates/server-cert.pem",
			ServerKeyPath:  "./certificates/server-key.pem",
		}
		_, err := creds.LoadServerCredentials()
		if err != nil {
			t.Errorf("unexpected error %v", err)
		}
		// TODO: What's a good way to check a "good" certificate?
	})
	t.Run("fails to parse a bad certificate file", func(t *testing.T) {
		creds := &utils.GpCredentials{
			CACertPath:     "./certificates/ca-cert.pem",
			CAKeyPath:      "./certificates/ca-key.pem",
			ServerCertPath: "./certificates/server-cert.pem",
			ServerKeyPath:  "./certificates/server-key.pem",
		}
		creds.ServerCertPath = "/dev/null"
		_, err := creds.LoadServerCredentials()
		if err == nil {
			t.Fatalf("expected TLS error, did not receive one")
		}
		if err.Error() != "could not load server credentials: tls: failed to find any PEM data in certificate input" {
			t.Errorf("expected TLS error, got %v", err)
		}
	})

	err = os.RemoveAll("./certificates")
	if err != nil {
		t.Fatalf("Cannot remove test certificates: %v", err)
	}
}

func TestLoadClientCredentials(t *testing.T) {
	err := exec.Command(constants.ShellPath, "-c", "../generate_test_tls_certificates.sh `hostname`").Run()
	if err != nil {
		t.Fatalf("Cannot generate test certificates: %v", err)
	}

	t.Run("successfully parses good certificate files", func(t *testing.T) {
		creds := &utils.GpCredentials{
			CACertPath:     "./certificates/ca-cert.pem",
			CAKeyPath:      "./certificates/ca-key.pem",
			ServerCertPath: "./certificates/server-cert.pem",
			ServerKeyPath:  "./certificates/server-key.pem",
		}
		_, err := creds.LoadClientCredentials()
		if err != nil {
			t.Errorf("unexpected error %v", err)
		}
		// TODO: What's a good way to check a "good" certificate?
	})
	t.Run("fails to parse a bad certificate file", func(t *testing.T) {
		creds := &utils.GpCredentials{
			CACertPath:     "./certificates/ca-cert.pem",
			CAKeyPath:      "./certificates/ca-key.pem",
			ServerCertPath: "./certificates/server-cert.pem",
			ServerKeyPath:  "./certificates/server-key.pem",
		}
		creds.CACertPath = "/dev/null"
		_, err := creds.LoadClientCredentials()
		if err == nil {
			t.Fatalf("expected TLS error, did not receive one")
		}
		if err.Error() != "failed to add server CA's certificate" {
			t.Errorf("expected TLS error, got %v", err)
		}
	})

	err = os.RemoveAll("./certificates")
	if err != nil {
		t.Fatalf("Cannot remove test certificates: %v", err)
	}
}
