package utils

import (
	"crypto/tls"
	"crypto/x509"
	"fmt"
	"os"

	"google.golang.org/grpc/credentials"
)

type Credentials interface {
	LoadServerCredentials() (credentials.TransportCredentials, error)
	LoadClientCredentials() (credentials.TransportCredentials, error)
}

type GpCredentials struct {
	CACertPath     string `json:"caCert"`
	CAKeyPath      string `json:"caKey"`
	ServerCertPath string `json:"serverCert"`
	ServerKeyPath  string `json:"serverKey"`
}

func (c GpCredentials) LoadServerCredentials() (credentials.TransportCredentials, error) {
	serverCert, err := tls.LoadX509KeyPair(c.ServerCertPath, c.ServerKeyPath)
	if err != nil {
		return nil, fmt.Errorf("could not load server credentials: %w", err)
	}

	config := &tls.Config{
		Certificates: []tls.Certificate{serverCert},
		ClientAuth:   tls.RequireAnyClientCert,
	}
	return credentials.NewTLS(config), nil

}

func (c GpCredentials) LoadClientCredentials() (credentials.TransportCredentials, error) {
	caCert, err := os.ReadFile(c.CACertPath)
	if err != nil {
		return nil, err
	}
	certPool := x509.NewCertPool()
	if !certPool.AppendCertsFromPEM(caCert) {
		return nil, fmt.Errorf("failed to add server CA's certificate")
	}

	clientCert, err := tls.LoadX509KeyPair(c.ServerCertPath, c.ServerKeyPath)
	if err != nil {
		return nil, fmt.Errorf("error while loading server certificate: %v", err)
	}

	config := &tls.Config{
		Certificates: []tls.Certificate{clientCert},
		RootCAs:      certPool,
	}

	return credentials.NewTLS(config), nil
}
