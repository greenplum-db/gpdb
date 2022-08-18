package main

import (
	"flag"
	"fmt"
	"os"
	"path"
)

var GLOBAL_CONF struct {
	GPHOME                 string
	GPDOCDIR               string
	HELP_DOC_NAME          string
	GP_PASSWD              string
	DEFAULT_QD_MAX_CONNECT int
	DEFAULT_BUFFERS        string
	BATCH_DEFAULT          int
}

func main() {
	gpinitsystemMain()
}

// gpinitsystemMain is the driving function
// Initializes variables and calls parser function to parse command line arguments
func gpinitsystemMain() {
	fmt.Println("[INFO]:-Start Main")
	initialize()
	// Parse the command line arguments
	parseCommandArguments()

	os.Exit(0)
}

// initialize function reads environment variables and initialize other vars
func initialize() {
	GLOBAL_CONF.GPHOME = os.Getenv("GPHOME")
	if GLOBAL_CONF.GPHOME == "" {
		fmt.Println("[FATAL]:-Environment variable $GPHOME not set")
		fmt.Println("[FATAL]:-Unable to continue")
		os.Exit(1)
	}
	GLOBAL_CONF.GPDOCDIR = path.Join(GLOBAL_CONF.GPHOME, "docs", "cli_help")
	GLOBAL_CONF.HELP_DOC_NAME = path.Join(GLOBAL_CONF.GPDOCDIR, "gpinitsystem_help")
	GLOBAL_CONF.GP_PASSWD = "abc'"
	GLOBAL_CONF.DEFAULT_QD_MAX_CONNECT = 250
	GLOBAL_CONF.DEFAULT_BUFFERS = "128000kB"
	GLOBAL_CONF.BATCH_DEFAULT = 60
}

// parseCommandArguments function parses command line arguments passed to the utility
// It calls required functions based on options provided.
func parseCommandArguments() {
	fmt.Printf("[INFO]:-Command line options passed to utility =%v\n", os.Args[1:])
	help := flag.Bool("help", false, "Show help")
	flag.Parse()
	if *help {
		printUsage()
		os.Exit(0)
	} else {
		fmt.Println("[ERROR]:- Unknown Parameter passed. Exiting..!")
	}
}

// printUsage function prints the usage message
// It checks if the help doc file exist and prints the file
// otherwise it prints static help message
func printUsage() {
	str_help := `
	      gpinitsystem -c gp_config_file [OPTIONS]
	
	      Creates a new Greenplum Database instance on a Coordinator host and a number of
	      segment instance hosts."
	
	      General options:
	      -v, display version information & exit
	
	      Logging options:"
	      -a, don't ask to confirm instance creation [default:- ask]
	      -D, set log output to debug level, shows all function calls
	      -l, logfile_directory [optional]
	          Alternative logfile directory
	      -q, quiet mode, do not log progress to screen [default:- verbose output to screen]
	
	      Configuration options:
	      -b, <size> shared_buffers per instance [default %s]
	          Specify either the number of database I/O buffers (without suffix) or the
	          amount of memory to use for buffers (with suffix 'kB', 'MB' or 'GB').
	          Applies to coordinator and all segments.
	      -B, <number> run this batch of create segment processes in parallel [default %d]
	      -c, gp_config_file [mandatory]
	          Supplies all Greenplum configuration information required by this utility.
	          Full description of all parameters contained within the example file
	          supplied with this distribution.
	          Also see file %s for greater detail on
	          the operation and configuration of this script
	      -e, <password>, password to set for Greenplum superuser in database [default %s]
	      -S, standby_datadir [optional]
	      -h, gp_hostlist_file [optional]
	          Contains a list of all segment instance hostnames required to participate in
	          the new Greenplum instance. Normally set in gp_config_file.
	      -I, <input_configuration_file>
	          The full path and filename of an input configuration file, which defines the
	          Greenplum Database members and segments using the QD_PRIMARY_ARRAY and
	          PRIMARY_ARRAY parameters. The input configuration file is typically created by
	          using gpinitsystem with the -O <output_configuration_file> option. You must
	          provide either the -c <cluster_configuration_file> option or the -I
	          <input_configuration_file> option to gpinitsystem."
	      -m, maximum number of connections for coordinator instance [default %d]
	      -n, <locale>, setting for locale to be set when database initialized [defaults to system locale]
	      -O, <output_configuration_file>
	          When used with the -O option, gpinitsystem does not create a new Greenplum
	          Database cluster but instead writes the supplied cluster configuration
	          information to the specified output_configuration_file. This file defines
	          Greenplum Database members and segments using the QD_PRIMARY_ARRAY and
	          PRIMARY_ARRAY parameters, and can be later used with -I
	          <input_configuration_file> to initialize a new cluster.
	      -p, postgresql_conf_gp_additions [optional]
	          List of additional PostgreSQL parameters to be applied to each Coordinator/Segment
	          postgresql.conf file during Greenplum database initialization.
	      -P, standby_port [optional]
	      -s, standby_hostname [optional]
	
	      Return codes:
	      0 - No problems encountered with requested operation
	      1 - Fatal error, instance not created/started, or in an inconsistent state,
	          see log file for failure reason.

    `
	if _, err := os.Stat(GLOBAL_CONF.HELP_DOC_NAME); err != nil {
		// error stating the help file, print standard help message
		fmt.Println("[WARNING]:- Error opening help file %v", GLOBAL_CONF.HELP_DOC_NAME)
		str_help = fmt.Sprintf(str_help, GLOBAL_CONF.DEFAULT_BUFFERS, GLOBAL_CONF.BATCH_DEFAULT, GLOBAL_CONF.HELP_DOC_NAME, GLOBAL_CONF.GP_PASSWD,
			GLOBAL_CONF.DEFAULT_QD_MAX_CONNECT)
		fmt.Println(str_help)
		os.Exit(0)
	}

	// if doc file present, print help from doc file
	content, err := os.ReadFile(GLOBAL_CONF.HELP_DOC_NAME)
	if err != nil {
		fmt.Println("[ERROR]:- Error opening help file %v", GLOBAL_CONF.HELP_DOC_NAME)
		str_help = fmt.Sprintf(str_help, GLOBAL_CONF.DEFAULT_BUFFERS, GLOBAL_CONF.BATCH_DEFAULT, GLOBAL_CONF.HELP_DOC_NAME, GLOBAL_CONF.GP_PASSWD,
			GLOBAL_CONF.DEFAULT_QD_MAX_CONNECT)
	}
	fmt.Println(string(content))

}
