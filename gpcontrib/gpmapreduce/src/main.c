#include <postgres_fe.h>
#include <libpq-fe.h>

#include <mapred.h>
#include <mapred_errors.h>
#include <except.h>

#include <stdlib.h>
#include <stdio.h>
#include <getopt_long.h>
#include <termios.h>
#include <signal.h>

ALLOW_EXCEPTIONS;

static char  VERSION[] =
	"Greenplum Map/Reduce Driver 1.00b2";

static char *wordchars =
	"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_";

/* Initialize global variables */
int             global_print_flag   = 0;
int             global_debug_flag   = 0;
int             global_verbose_flag = 0;
int             global_explain_flag = 0;
mapred_plist_t *global_plist        = NULL;

/* libPQ cancel context */
static PGcancel *volatile cancelConn = NULL;


void usage(char *procname, boolean full);
void showVersion(char *procname);
int  main(int argc, char *argv[]);
void read_password(char *p, size_t len);
void check_version(PGconn *conn);

static void sigint_handler(SIGNAL_ARGS);

/* usage() - print usage information */
void usage(char *procname, boolean full)
{
	if (full)
	{
		showVersion(procname);
		printf("\nUsage:\n  ");
	}

	printf("%s [options] -f file.yml [dbname [username]]\n", procname);

	if (full)
	{
		printf(
			"\n"
			"General options:\n"
			"  -? | --help                   show this help, then exit\n"
			"  -V | --version                show version information, then exit\n"
			"  -v | --verbose                verbose output\n"
			"  -x | --explain                do not run jobs, but produce explain plans\n"
			"  -X | --explain-analyze        run jobs and produce explain-analyze plans\n"
			"  -k | --key <name>=<value>     sets a yaml variable\n"
			"\n"
			"Connection options:\n"
			"  -h | --host <hostname>        database server host or socket directory\n"
			"  -p | --port <port>            database server port\n"
			"  -U | --username <username>    database user name\n"
			"  -W | --password               prompt for password\n"
			"\n"
			"Debug options:\n"
			"  -D | --debug                  enable some debugging output\n"
			"  -P | --print                  print-only mode, do not run jobs\n"
			);
	}
}

void showVersion(char *procname)
{
	printf("%s - %s\n", procname, VERSION);
}

int main (int argc, char *argv[])
{
	volatile int       errcode = 0;
	int                c;
	char              *procname = argv[0];
	int                forceprompt = false;
	int                needpass = false;
	char              *filename = NULL;
	char              *username = NULL;
	char              *database = NULL;
	char              *hostname = NULL;
	char              *port     = NULL;
	mapred_olist_t    *documents;
	mapred_olist_t    *doc_item;
	FILE              *file;

	/* The long_options structure */
	static struct option long_options[] = {
		{"help",     no_argument,       0, '?'},
		{"version",  no_argument,       0, 'V'},
		{"verbose",  no_argument,       0, 'v'},
		{"password", no_argument,       0, 'W'},
		{"explain",  no_argument,       0, 'x'},
		{"explain-analyze", no_argument, 0, 'X'},

		{"username", required_argument, 0, 'U'},
		{"host",     required_argument, 0, 'h'},
		{"port",     required_argument, 0, 'p'},
		{"file",     required_argument, 0, 'f'},
		{"key",      required_argument, 0, 'k'},
		{"print",    no_argument,       0, 'P'},
		{"debug",    no_argument,       0, 'D'},
		{0, 0, 0, 0}
	};

	static char* short_options = "VvWxXU:h:p:f:k:?PD";

	while (1)
	{
		int option_index = 0;
		c = getopt_long(argc, argv, short_options, long_options,
						&option_index);
		if (c == -1)
			break; /* done processing options */
		switch (c)
		{
			case '?':  /* --help */
				
				/* Actual help option given */
				if (strcmp(argv[optind - 1], "-?") == 0 ||
					strcmp(argv[optind - 1], "--help") == 0)
				{
					usage(procname, true);
					exit(0);
				}
				
				/* unknown option reported by getopt */
				fprintf(stderr, "Try \"%s --help\" for usage information.\n",
						procname);
				exit(1);
				
			case 'V':  /* --version */
				showVersion(procname);
				exit(0);

			case 'v':  /* --verbose */
				global_verbose_flag = true;
				break;

			case 'x': /* --explain */
				global_explain_flag |= global_explain;
				break;

			case 'X': /* --explain-analyze */
				global_explain_flag |= global_explain | global_analyze;
				break;

			case 'P':  /* --print */
				global_print_flag = 1;
				break;

			case 'D':  /* --debug */
				global_debug_flag = 1;
				break;

			case 'W':  /* --password */
				forceprompt = true;
				break;

			case 'U':  /* --username */
				username = optarg;
				break;

			case 'h':  /* --host */
				hostname = optarg;
				break;

			case 'p':  /* --port */
				port = optarg;
				break;

			case 'f':  /* --file */
				filename = optarg;
				break;

			case 'k':  /* --key */
			{
				mapred_plist_t *newitem;
				char *name = optarg;
				char *value = NULL;
				char *eq = strchr(name, '=');

				/*
				 * either --key value      : sets parameter named "key"
				 * or     --key name=value : sets parameter named "name"
				 */
				if (eq)
				{
					eq[0] = '\0';
					value = eq+1;
					
					/* make sure parameter is a valid name */
					if (strspn(name, wordchars) != strlen(name))
					{
						fprintf(stderr, "bad parameter --key %s\n", name);
						exit(1);
					}
				}
				else
				{
					value = name;
					name = "key";
				}

				/* Add the parameter to the global parameter list */
				newitem = malloc(sizeof(mapred_plist_t));
				newitem->name  = name;
				newitem->type  = value;
				newitem->next  = global_plist;
				global_plist   = newitem;
			}
			break;

			default:  /* not feasible */
				fprintf(stderr, "Error processing options\n");
				exit(1);
		}
	}

	/* open the file */
	if (!filename)
	{
		usage(procname, false);
		exit(1);
	}

	file = fopen(filename, "rb");
	if (!file)
	{
		fprintf(stderr, "Error: Could not open file '%s'\n", filename);
		exit(1);
	}

	/*
	 * Handle additional arguments as would psql:
	 *   - First argument is database
	 *   - Second argument is username, if not specified via -U
	 *   - All other arguments generate warnings
	 */
	if (optind < argc && !database)
		database = argv[optind++];
	if (optind < argc && !username)
		username = argv[optind++];
	while (optind < argc)
	{
		fprintf(stderr, "%s: warning: extra command-line argument \"%s\" ignored\n",
				procname, argv[optind++]);
	}

	if (global_verbose_flag)
	{
		mapred_plist_t *param = global_plist;
		while (param)
		{
			fprintf(stderr, "- Parameter: %s=%s\n",
					param->name, param->type);
			param = param->next;
		}
		fprintf(stderr, "- Parsing '%s':\n", filename);
	}
	documents = NULL;
	XTRY
	{
		documents = mapred_parse_file(file);
	}
	XCATCH(ASSERTION_FAILURE)
	{
		fprintf(stderr, "Assertion failure at %s:%d\n",
				xframe.file, xframe.lineno);	
		exit(1);
	}
	XCATCH_ANY
	{
		if (global_verbose_flag)
			fprintf(stderr, "  - ");
		if (xframe.exception)
			fprintf(stderr, "Error: %s\n", (char *) xframe.exception);
		else
			fprintf(stderr, "Unknown Error (%d) at %s:%d\n",
					xframe.errcode, xframe.file, xframe.lineno);
		exit(1);
	}
	XTRY_END;

	/* Do something interesting with documents */
	for (doc_item = documents; doc_item; doc_item = doc_item->next)
	{
		PGconn   *conn = NULL;
		char      pwdbuf[100];
		char      portbuf[11];  /* max int size should be 10 digits */
		char     *user, *db, *host, *pwd, *pqport, *options, *tty;

		XTRY
		{
			mapred_document_t *doc = &doc_item->object->u.document;

			if (global_verbose_flag)
			{
				fprintf(stderr, "- Executing Document %d:\n", doc->id);
			}

			if (port)
			{
				pqport = port;
			}
			else if (doc->port > 0)
			{
				snprintf(portbuf, sizeof(portbuf), "%d", doc->port);
				pqport = portbuf;
			}
			else
			{
				pqport = NULL;
			}
			if (database)
				db = database;
			else
				db = doc->database;
			if (username)
				user = username;
			else
				user = doc->user;
			if (hostname)
				host = hostname;
			else
				host = doc->host;

			options = NULL;
			tty     = NULL;
			pwd     = NULL;

			if (forceprompt)
			{
				read_password(pwdbuf, sizeof(pwdbuf));
				pwd = pwdbuf;
			}

			do {
				conn = PQsetdbLogin(host, pqport, options, tty, db, user, pwd);

				needpass = false;
				if (PQstatus(conn) == CONNECTION_BAD &&
					!strcmp(PQerrorMessage(conn), PQnoPasswordSupplied))
				{
					PQfinish(conn);

					read_password(pwdbuf, sizeof(pwdbuf));
					pwd = pwdbuf;
					needpass = true;
				}
			} while (needpass);

			if (PQstatus(conn) == CONNECTION_BAD)
			{
				XRAISE(CONNECTION_ERROR, PQerrorMessage(conn));
			}
			else
			{
				if (global_verbose_flag)
				{
					fprintf(stderr, "  - Connected Established:\n");
					fprintf(stderr, "    HOST: %s\n",
							PQhost(conn) ? PQhost(conn) : "localhost");
					fprintf(stderr, "    PORT: %s\n", PQport(conn));
					fprintf(stderr, "    USER: %s/%s\n", PQuser(conn), PQdb(conn));
				}
				check_version(conn);
				
				/* Prepare to receive interrupts */
				cancelConn = PQgetCancel(conn);

				if (signal(SIGINT, sigint_handler) == SIG_IGN)
					signal(SIGINT, SIG_IGN);
				if (signal(SIGHUP, sigint_handler) == SIG_IGN)
					signal(SIGHUP, SIG_IGN);
				if (signal(SIGTERM, sigint_handler) == SIG_IGN)
					signal(SIGTERM, SIG_IGN);

				mapred_run_document(conn, doc);
			}
		}
		XCATCH(ASSERTION_FAILURE)
		{
			fprintf(stderr, "Assertion failure at %s:%d\n",
					xframe.file, xframe.lineno);	
			errcode = 1;
		}
		XCATCH(USER_INTERUPT)
		{
			if (global_verbose_flag)
				fprintf(stderr, "  - ");			
			fprintf(stderr, "Job Cancelled: User Interrupt");
			exit(2); /* exit immediately */
		}
		XCATCH_ANY
		{
			if (global_verbose_flag)
				fprintf(stderr, "  - ");
			if (xframe.exception)
				fprintf(stderr, "Error: %s\n", (char *) xframe.exception);
			else
				fprintf(stderr, "Unknown Error (%d) at %s:%d\n",
						xframe.errcode, xframe.file, xframe.lineno);
			errcode = 1;
		}
		XFINALLY
		{
			/* Ignore signals until we exit */
			signal(SIGINT, SIG_IGN);
			signal(SIGHUP, SIG_IGN);
			signal(SIGTERM, SIG_IGN);

			PQfreeCancel(cancelConn);
			cancelConn = NULL;
			PQfinish(conn);
		}
		XTRY_END;
	}
	
	/* Cleanup */
	mapred_destroy_olist(&documents);
	fclose(file);

	return errcode;
}


void read_password(char *p, size_t len)
{
    struct termios t_orig, t;
    FILE *termin, *termout;

    termin  = fopen("/dev/tty", "r");
    termout = fopen("/dev/tty", "w");
    if (!termin || !termout)
    {
        if (termin)
			fclose(termin);
        if (termout)
            fclose(termout);
        termin = stdin;
        termout = stdout;
    }

    tcgetattr(fileno(termin), &t);
    t_orig = t;
    t.c_lflag &= ~ECHO;
    tcsetattr(fileno(termin), TCSAFLUSH, &t);
	
    fputs("Password> ", termout);
    fflush(termout);

    if (fgets(p, len, termin) == NULL)
        p[0] = '\0';

    /* If that didn't get the whole input, drain the rest */
    len = strlen(p);
    if (len > 0)
    {
        if (p[len-1] == '\n')
            p[len-1] = '\0';
        else
        {
            char c[25];
            do {
                if (fgets(c, sizeof(c), termin) == NULL)
                    break;
                len = strlen(c);
            } while (len > 0 && c[len-1] != '\n');
        }
    }

    tcsetattr(fileno(termin), TCSAFLUSH, &t_orig);
    fputs("\n", termout);
    fflush(termout);

    if (termin != stdin)
    {
        fclose(termin);
        fclose(termout);
    }
}


void check_version(PGconn *conn)
{
	PGresult *result;

	result = PQexec(conn, "select (regexp_matches(version(), "
					"E'\\\\(Greenplum Database ([^)]+)\\\\)'))[1]");

	if (PQresultStatus(result) == PGRES_TUPLES_OK && PQntuples(result) == 1)
	{
		char *version = PQgetvalue(result, 0, 0);

		if (global_verbose_flag)
			fprintf(stderr, "    VERSION: %s\n", version);

		/* Determine if this version is approved for mapreduce usage */
		if (strncmp(version, "main", 4) != 0)
		{
			/* Get major and minor release, patch level is ignored */
			char *dot   = NULL;
			int   major = strtol(version, &dot, 10);
			int   minor = strtol(dot+1, NULL, 10);
			if (major < 3 || (major == 3 && minor < 2))
			{
				char buf[100];
				snprintf(buf, sizeof(buf),
						 "Unsupported backend version: %s", version);
				XRAISE(VERSION_ERROR, strdup(buf));
			}
		}
	}
	else
	{
		fprintf(stderr, "%s", PQerrorMessage(conn));
		XRAISE(MAPRED_SQL_ERROR, "Unable to determine Greenplum version\n");
	}

	PQclear(result);
}


/*
 * sigint_handler() -  Interrupt handler to cancel active queries
 */
static void sigint_handler(SIGNAL_ARGS)
{
	char errbuf[256];

	/* Once we accept an interrupt it's best to simply disable the handler */
	signal(SIGINT, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGTERM, SIG_IGN);

	if (cancelConn != NULL)
	{
		if (PQcancel(cancelConn, errbuf, sizeof(errbuf)))
			fprintf(stderr, "Cancel request sent\n");
		else
		{
			fprintf(stderr, "Could not send cancel request: ");
			fprintf(stderr, "%s", errbuf);
		}
	}

	XRAISE(USER_INTERUPT, "SIGINT");
}
