package main

import (
	"fmt"
	"io/ioutil"
	"net"

	"os/exec"

	"io"

	"os"

	"errors"
	"strings"

	"log"

	"golang.org/x/crypto/ssh"
)

var (
	gConfig *ssh.ServerConfig
)

func startShell(channel ssh.Channel, requests <-chan *ssh.Request) {

	go func(in <-chan *ssh.Request) {
		defer channel.Close()
		for req := range in {
			payload := string(req.Payload)
			switch req.Type {
			case "exec":
				// todo handle error
				cmdName, _ := parsePayload(payload)
				fmt.Println("ssh payload: " + cmdName)
				cmd := exec.Command("bash", "-c", fmt.Sprintf("%s", cmdName))

				stdout, err := cmd.StdoutPipe()
				exitcode := []byte{0, 0, 0, 0}

				// TODO: We need to somehow pass this here so that we know the results based on test
				if cmdName == "respond that pg_upgrade isn't running" {
					// store in some data structure
				} else {
					// look the command up in the data structure to see if we can respond to it
				}
				if cmdName == "ps auxx | grep pg_upgrade" {

					stdout = ioutil.NopCloser(strings.NewReader("pg_upgrade is not running on host 'localhost', segment_id '42'"))
					exitcode = []byte{0, 0, 0, 0}
				}

				//TODO: Check if read is empty

				if err != nil && err != io.EOF {
					panic("cannot get stdout")
				}
				stderr, err := cmd.StderrPipe()
				if err != nil {
					panic("cannot get stderr")
				}
				input, err := cmd.StdinPipe()
				if err != nil {
					panic("cannot get stdin")
				}

				if err = cmd.Start(); err != nil {
					panic("cannot start command")
				}

				req.Reply(true, nil) //because the channel is already set up, the payload doesn't matter

				go io.Copy(input, channel)
				io.Copy(channel, stdout)
				io.Copy(channel.Stderr(), stderr)

				if err = cmd.Wait(); err != nil {
					panic("cannot wait for command")
				}

				channel.SendRequest("exit-status", false, exitcode) //payload is a big endian encoded uint32 that is the value of the exit status

				return
			default:
				//	only handle one-off "exec" requests
			}
		}
	}(requests)
}

func serviceSSHChannel(channel ssh.Channel, requests <-chan *ssh.Request) {
	startShell(channel, requests)
}

func serviceSSHConnection(newSSHChannelReq <-chan ssh.NewChannel) {
	for newChannel := range newSSHChannelReq {
		if newChannel.ChannelType() != "session" {
			newChannel.Reject(ssh.UnknownChannelType, "unknown channel type")
			continue
		}

		channel, requests, err := newChannel.Accept()
		if err != nil {
			fmt.Println("Accept2 err: ", err)
			continue
		}

		go serviceSSHChannel(channel, requests)
	}
}

func handshakeSocket(newConn net.Conn) {
	_, chans, reqs, err := ssh.NewServerConn(newConn, gConfig)
	if err != nil {
		fmt.Println("Err NewServerConn: ", err)
		return
	}

	go ssh.DiscardRequests(reqs) // The incoming Request channel must be serviced.
	serviceSSHConnection(chans)
}

func listenerForever() {
	listener, err := net.Listen("tcp", ":2022")
	if err != nil {
		panic(err)
	}

	for {
		// accept routine
		newConn, err := listener.Accept()
		if err != nil {
			fmt.Println("Accept err: ", err)
			continue
		}
		go handshakeSocket(newConn)
	}
}

func passwordCheck(c ssh.ConnMetadata, pass []byte) (*ssh.Permissions, error) {
	// Should use constant-time compare (or better, salt+hash) in a production setting.
	if c.User() == "testuser" && string(pass) == "pass" {
		return nil, nil
	}
	fmt.Printf("User %s with password %s\n", c.User(), string(pass))
	return nil, fmt.Errorf("password rejected for %q", c.User())
}

//func privateKeyCheck(c ssh.ConnMetadata) (*ssh.Permissions, error) {
//	// Should use constant-time compare (or better, salt+hash) in a production setting.
//	if c.User() == "testuser" && string(pass) == "pass" {
//		return nil, nil
//	}
//	fmt.Printf("User %s with password %s\n", c.User(), string(pass))
//	return nil, fmt.Errorf("password rejected for %q", c.User())
//}

func startSshServer() {
	// todo use local file

	authorizedKeysBytes, err := ioutil.ReadFile(os.Getenv("GOPATH") + "/src/gp_upgrade/commands/sshd/authorized_keys")
	if err != nil {
		log.Fatalf("Failed to load authorized_keys, err: %v", err)
	}

	authorizedKeysMap := map[string]bool{}
	for len(authorizedKeysBytes) > 0 {
		pubKey, _, _, rest, err := ssh.ParseAuthorizedKey(authorizedKeysBytes)
		if err != nil {
			log.Fatal(err)
		}

		authorizedKeysMap[string(pubKey.Marshal())] = true
		authorizedKeysBytes = rest
	}

	gConfig = &ssh.ServerConfig{
		//PasswordCallback: passwordCheck,

		// Remove to disable public key auth.
		PublicKeyCallback: func(c ssh.ConnMetadata, pubKey ssh.PublicKey) (*ssh.Permissions, error) {
			if authorizedKeysMap[string(pubKey.Marshal())] {
				return nil, nil
			}
			return nil, fmt.Errorf("unknown public key for %q", c.User())
		},
	}

	// todo change to find via being next to this file
	pBytes, err := ioutil.ReadFile(os.Getenv("GOPATH") + "/src/gp_upgrade/commands/sshd/private_key.pem")
	if err != nil {
		panic(err)
	}

	private, err := ssh.ParsePrivateKey(pBytes)
	if err != nil {
		panic(err)
	}
	gConfig.AddHostKey(private)

	listenerForever()
}

func parsePayload(payload string) (string, error) {
	payloadUTF8 := strings.Map(func(r rune) rune {
		if r >= 32 && r < 127 {
			return r
		}
		return -1
	}, payload)

	prefix := ""
	if prefIdx := strings.Index(payloadUTF8, prefix); prefIdx != -1 {
		p := strings.TrimSpace(payloadUTF8[prefIdx+len(prefix):])
		return p, nil
	}
	return "", errors.New("cannot handle command: " + payload)
}

func main() {
	if len(os.Args) > 1 && "--dry-run" == os.Args[1] {
		fmt.Println("ssh server dry run")
		return
	}
	fmt.Println("Starting ssh server on port 2022")
	startSshServer()
}
