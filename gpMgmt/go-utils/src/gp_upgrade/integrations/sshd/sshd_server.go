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

	"path"
	"runtime"

	"gp_upgrade/test_utils"

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
				cmdName, err := parsePayload(payload)
				test_utils.Check("Cannot parse payload", err)

				cmd := exec.Command("bash", "-c", fmt.Sprintf("%s", cmdName))

				stdout, err := cmd.StdoutPipe()
				test_utils.Check("Cannot get stdoutpipe", err)

				var cheatSheet test_utils.CheatSheet
				//TODO: Currently reading from a cheatsheet file; possibly passing through ssh server instead?
				err = cheatSheet.ReadFromFile()
				test_utils.Check("Cannot read from file", err)

				// NOTE: We are intentionally overwriting the bash command output
				// Probably not necessary to actually run the command anymore...
				stdout = ioutil.NopCloser(strings.NewReader(cheatSheet.Response))
				exitcode := cheatSheet.ReturnCode

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

func startSshServer() {
	_, this_file_path, _, _ := runtime.Caller(0)
	sshd_directory := path.Dir(this_file_path)
	authorizedKeysBytes, err := ioutil.ReadFile(path.Join(sshd_directory, "authorized_keys"))
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

	pBytes, err := ioutil.ReadFile(path.Join(sshd_directory, "private_key.pem"))
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
	if os.Getenv("GOPATH") == "" {
		fmt.Println("GOPATH is not set. Cannot start sshd server.")
		os.Exit(1)
	}
	startSshServer()
}
