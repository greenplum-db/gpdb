package configure

import (
	"os"
	"strings"
	"testing"

	"github.com/greenplum-db/gpdb/gp/test/integration/testutils"
)

func TestConfigureFailure(t *testing.T) {
	// creating empty hostfile for test
	_ = os.WriteFile(mockHostFile, []byte(""), 0644)

	var ConfigureFailTestCases = []struct {
		name        string
		cliParams   []string
		expectedOut []string
	}{
		{
			name:      "configure service with empty value for --host option",
			cliParams: []string{"--host", ""},
			expectedOut: []string{
				"please provide a valid input host name",
			},
		},
		{
			name:      "configure service with no value for --host option",
			cliParams: []string{"--host"},
			expectedOut: []string{
				"flag needs an argument: --host",
			},
		},
		{
			name:      "configure service with empty file for --hostfile option",
			cliParams: []string{"--hostfile", "hostlist"},
			expectedOut: []string{
				"expected at least one host or hostlist specified",
			},
		},
		{
			name:      "configure service with no value for --hostfile option",
			cliParams: []string{"--hostfile"},
			expectedOut: []string{
				"flag needs an argument: --hostfile",
			},
		},
		{
			name:      "configure service with non-existing host for --host option",
			cliParams: []string{"--host", "host"},
			expectedOut: []string{
				"could not copy gp.conf file to segment hosts",
			},
		},
		{
			name:      "configure service with one valid host and invalid host for --host option",
			cliParams: []string{"--host", testutils.DefaultHost, "--host", "invalid"},
			expectedOut: []string{
				"could not copy gp.conf file to segment hosts",
			},
		},
		{
			name: "configure service without any option",
			expectedOut: []string{
				"at least one hostname must be provided using either --host or --hostfile",
			},
		},
		{
			name:      "configure service with invalid option",
			cliParams: []string{"--invalid"},
			expectedOut: []string{
				"unknown flag: --invalid",
			},
		},
		{
			name: "configure service with both host and hostfile options",
			cliParams: []string{"--host", testutils.DefaultHost,
				"--hostfile", "abc"},
			expectedOut: []string{
				"[ERROR] if any flags in the group [host hostfile] are set none of the others can be; [host hostfile] were all set",
			},
		},
		{
			name: "configure service with string value for --agent-port option",
			cliParams: []string{"--host", testutils.DefaultHost,
				"--agent-port", "abc"},
			expectedOut: []string{
				"invalid argument",
			},
		},
		{
			name: "configure service with string value for --hub-port option",
			cliParams: []string{"--host", testutils.DefaultHost,
				"--hub-port", "abc"},
			expectedOut: []string{
				"invalid argument",
			},
		},
		{
			name: "configure service with no value for --agent-port option",
			cliParams: []string{"--host", testutils.DefaultHost,
				"--agent-port"},
			expectedOut: []string{
				"flag needs an argument: --agent-port",
			},
		},
		{
			name: "configure service with no value for --hub-port option",
			cliParams: []string{"--host", testutils.DefaultHost,
				"--hub-port"},
			expectedOut: []string{
				"flag needs an argument: --hub-port",
			},
		},
		{
			name: "configure service with non-existing directory as service-dir value",
			cliParams: []string{
				"--host", testutils.DefaultHost,
				"--service-dir", "/newDir/Service-dir",
			},
			expectedOut: []string{
				"could not create service directory /newDir/Service-dir on hosts",
			},
		},
		{
			name: "configure service with no value for service-dir option",
			cliParams: []string{
				"--host", testutils.DefaultHost,
				"--service-dir",
			},
			expectedOut: []string{
				"flag needs an argument: --service-dir",
			},
		},
		{
			name: "configure service with no value for log-dir option",
			cliParams: []string{
				"--host", testutils.DefaultHost,
				"--log-dir",
			},
			expectedOut: []string{
				"flag needs an argument: --log-dir",
			},
		},
		{
			name: "configure fails when value for both --agent-port and --hub-port are same",
			cliParams: []string{
				"--host", testutils.DefaultHost,
				"--agent-port", "2000",
				"--hub-port", "2000",
			},
			expectedOut: []string{
				"[ERROR] hub port and agent port must be different",
			},
		},
		{
			name: "configure service fails when --gpHome value is invalid",
			cliParams: []string{
				"--host", testutils.DefaultHost,
				"--gpHome", "invalid",
			},
			expectedOut: []string{
				"could not create configuration file invalid/gp.conf",
			},
		},
		{
			name: "configure service fails when --gpHome value is empty",
			cliParams: []string{
				"--host", testutils.DefaultHost,
				"--gpHome", "",
			},
			expectedOut: []string{
				"not a valid gpHome found",
			},
		},
		{
			name: "configure service fails when no value given for --gpHome",
			cliParams: []string{
				"--host", testutils.DefaultHost,
				"--gpHome",
			},
			expectedOut: []string{
				"flag needs an argument: --gpHome",
			},
		},
		{
			name: "configure fails when non-existing service user is given",
			cliParams: []string{
				"--host", testutils.DefaultHost,
				"--service-user", "user"},
			expectedOut: []string{
				"could not create service directory",
			},
		},
	}

	for _, tc := range ConfigureFailTestCases {
		t.Run(tc.name, func(t *testing.T) {
			result, err := testutils.RunConfigure(true, tc.cliParams...)
			if err == nil {
				t.Errorf("\nExpected error Got: %#v", err)
			}
			if result.ExitCode != testutils.ExitCode1 {
				t.Errorf("\nExpected: %#v \nGot: %v", testutils.ExitCode1, result.ExitCode)
			}
			for _, item := range tc.expectedOut {
				if !strings.Contains(result.OutputMsg, item) {
					t.Errorf("\nExpected string: %#v \nNot found in: %#v", item, result.OutputMsg)
				}
			}

			// check for panic error
			if strings.Contains(result.OutputMsg, "panic") {
				t.Errorf("\nUnexpected string: %#v \nFound in: %#v", "panic", result.OutputMsg)
			}
		})
	}
}
