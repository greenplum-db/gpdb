#include "postgres.h"

#include "funcapi.h"
#include "tablefuncapi.h"
#include "miscadmin.h"
#include "catalog/catalog.h"
#include "catalog/pg_tablespace.h"
#include "cdb/cdbvars.h"
#include "utils/builtins.h"
#include "utils/memutils.h"

#include <unistd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <regex.h>

/**
 * The use pattern is
 * select gp_tablespace_inotify_cmd('init0');
 * select gp_tablespace_inotify_cmd('add', '<name1>', '<directory of tablespace>');
 * select gp_tablespace_inotify_cmd('add', '<name2>', '<directory of tablespace>');
 * ...
 * select gp_tablespace_inotify_cmd('add', '<nameN>', '<directory of tablespace>');
 *
 * Run queries
 *
 * -- Collect inotify events and stop receiving any further events.
 * select gp_tablespace_inotify_cmd('checkout');
 *
 * -- Check if some files were created in the above queries. <nameX> is used
 * -- to reference the tablespace.
 * select gp_tablespace_inotify_match('<nameX>', '<pattern_of_file_name>')
 * select gp_tablespace_inotify_match('<nameY>', '<pattern_of_file_name>')
 *
 * -- optional to clear the state
 * select gp_tablespace_inotify_cmd('clear');
 * -- After `clear`, you can run another queries and do some check.
 * -- You don't need to setup the tablespaces again.
 *
 * -- Release inotify and other resources
 * select gp_tablespace_inotify_cmd('exit');
 *
 */


static int tablespace_inotify_fd = -1;
static FILE *logFILE = NULL;
struct tablespace_info {
	int wd;
	char *name;
	char *location;
	char **ev_names;
	int num_evnames;
	int cap_evnames;
};
static int num_ts_info = 0;
static struct tablespace_info ts_info_list[16];
static MemoryContext mc = NULL;

static void tablespace_inotify_checks(void);

static void
tablespace_inotify_init0()
{
	int fd;
	if (tablespace_inotify_fd >= 0)
	{
		close(tablespace_inotify_fd);
		tablespace_inotify_fd = -1;
	}
	if (logFILE == NULL)
	{
		char filename[128];
		snprintf(filename, sizeof(filename), "/tmp/gp_tablespace_inotify_dbid%d.log",
				GpIdentity.dbid);
		logFILE = fopen(filename, "a+");
		if (logFILE == NULL)
			ereport(ERROR, (errmsg("Can't open gp_tablespace_inotify.log")));
	}
	num_ts_info = 0;
	if (mc)
		MemoryContextReset(mc);
	else
		mc = AllocSetContextCreate(TopMemoryContext, "table_spaces_test",
									ALLOCSET_DEFAULT_MINSIZE,
									ALLOCSET_DEFAULT_INITSIZE,
									ALLOCSET_DEFAULT_MAXSIZE);
	fd = inotify_init1(IN_NONBLOCK);
	if (fd < 0)
		ereport(ERROR, (errmsg("Unable to allocate inotify FD:%m")));
	tablespace_inotify_fd = fd;
}
static int
tablespace_inotify_clear(const char *name)
{
	int num = 0;
	bool all = strcmp(name, "*") == 0;
	for (int i = 0; i < num_ts_info; i++) {
		struct tablespace_info *ts = &ts_info_list[i];
		if (ts->num_evnames > 0 && (all || strcmp(name, ts->name) == 0))
		{
			for (int idx = ts->num_evnames - 1; idx >= 0; idx--)
				pfree(ts->ev_names[idx]);
			pfree(ts->ev_names);
			ts->ev_names = NULL;
			ts->num_evnames = ts->cap_evnames = 0;

			num++;
		}
	}
	return num;
}
static void
tablespace_inotify_exit()
{
	num_ts_info = 0;
	if (tablespace_inotify_fd >= 0)
	{
		close(tablespace_inotify_fd);
		tablespace_inotify_fd = -1;
	}
	if (mc)
	{
		MemoryContext tmp = mc;
		mc = NULL;
		MemoryContextDelete(tmp);
	}
	if (logFILE)
	{
		FILE *tmp = logFILE;
		logFILE = NULL;
		fclose(tmp);
	}
}

static void
tablespace_inotify_checks()
{
	if (tablespace_inotify_fd < 0)
		ereport(ERROR, (errmsg("inotify fd is uninitialized")));
	if (mc == NULL)
		ereport(ERROR, (errmsg("memory context is null")));

}
// tablespace location, database
static void
tablespace_inotify_add(const char *name, const char *location)
{
	struct tablespace_info *tsi;
	MemoryContext old;
	const char *path;
	int wd;

	tablespace_inotify_checks();
	elog(LOG, "name='%s'", name);
	elog(LOG, "location='%s'", location);

	if (num_ts_info >= 16)
		ereport(ERROR, (errmsg("Too many tablespace to watch")));

	path = location;
	// create pgsql_tmp if not exists.
	{
		struct stat stats;
		int e;
		e = stat(path, &stats);
		if (e != 0)
		{
			if (errno != ENOENT)
				ereport(ERROR, (errmsg("Can't access path='%s', %m", path)));
			e = mkdir(path, 0700);
			if (e != 0)
				ereport(ERROR, (errmsg("Can't create temp directory:%m")));
		}
//		if (!S_ISDIR(stats.st_mode))
//			ereport(ERROR, (errmsg("Temp path is not a directory")));
	}
	wd = inotify_add_watch(tablespace_inotify_fd, path, IN_CREATE);
	if (wd < 0)
		ereport(ERROR, (errmsg("Can't add watch for path:%s, %m", path)));

	ereport(LOG, (errmsg("inotify path:='%s'", path)));
	old = MemoryContextSwitchTo(mc);
	tsi = &ts_info_list[num_ts_info];
	tsi->wd = wd;
	tsi->name = pstrdup(name);
	tsi->location = pstrdup(path);
	tsi->ev_names = NULL;
	tsi->num_evnames = tsi->cap_evnames = 0;
	MemoryContextSwitchTo(old);
	num_ts_info++;

}

// consume all CREATE events.
// FIXME: inotify events may be dropped because of too many incoming inotify events
// 		before we read them.
static void
tablespace_inotify_checkout()
{
	char buffer[8192];
	const struct inotify_event *ev;
	int i, n;
	MemoryContext old;

	tablespace_inotify_checks();
	old = MemoryContextSwitchTo(mc);

	while ((n = read(tablespace_inotify_fd, buffer, sizeof(buffer))) > 0) {
		for (char *ptr = &buffer[0];
				ptr < buffer + n;
				ptr += sizeof(*ev) + ev->len) {
			struct tablespace_info *ts;
			ev = (const struct inotify_event *)ptr;
			for (i = 0; i < num_ts_info; i++) {
				if (ts_info_list[i].wd == ev->wd)
					break;
			}
			if (i == num_ts_info)
			{
				fprintf(logFILE, "[unamed]: evname = '%s' [%s]\n",
						ev->name, ev->mask & IN_ISDIR ? "directory" : "file");
				continue;
			}

			ts = &ts_info_list[i];
			if (ts->num_evnames >= ts->cap_evnames) {
				int cap = ts->cap_evnames < 4 ? 8 : ts->cap_evnames * 2;
				if (ts->ev_names)
					ts->ev_names = (char **)repalloc(ts->ev_names, cap * sizeof(char*));
				else
					ts->ev_names = (char **)palloc(cap * sizeof(char*));

				ts->cap_evnames = cap;
			}
			ts->ev_names[ts->num_evnames++] = pstrdup(ev->name);
			fprintf(logFILE, "[%s]: %s | %s [%s]\n",
					ts->name, ts->location, ev->name,
					ev->mask & IN_ISDIR ? "directory" : "file");
		}
	}
	MemoryContextSwitchTo(mc);
}


PG_FUNCTION_INFO_V1(gp_tablespace_inotify_match);
Datum
gp_tablespace_inotify_match(PG_FUNCTION_ARGS)
{
	regex_t reg;
	bool found = false;
	int num_matched = 0;
	char *name = TextDatumGetCString(PG_GETARG_TEXT_PP(0));
	char *pat = TextDatumGetCString(PG_GETARG_TEXT_PP(1));
	if (regcomp(&reg, pat, REG_EXTENDED|REG_NOSUB) != 0)
		ereport(ERROR, (errmsg("Bad pattern:")));

	for (int i = 0; i < num_ts_info; i++) {
		const struct tablespace_info *ts = &ts_info_list[i];
		if (strcmp(ts->name, name) != 0)
			continue;
		found = true;
		for (int k = 0; k < ts->num_evnames; k++) {
			if (regexec(&reg, ts->ev_names[k], 0, NULL, 0) == 0)
				num_matched++;
		}
	}
	regfree(&reg);
	if (!found)
		ereport(LOG, (errmsg("Doesn't find name='%s'", name)));
	return num_matched;
	PG_RETURN_INT32(num_matched);
}

PG_FUNCTION_INFO_V1(gp_tablespace_inotify_cmd);
Datum
gp_tablespace_inotify_cmd(PG_FUNCTION_ARGS)
{
	const char *cmd = TextDatumGetCString(PG_GETARG_TEXT_PP(0));
	char *name = NULL;

	if (cmd == NULL || *cmd == '\0')
		ereport(ERROR, (errmsg("%s: Invalid arguments", __func__)));
	if (PG_NARGS() > 1)
		name = TextDatumGetCString(PG_GETARG_TEXT_PP(1));

	if (strcmp(cmd, "init0") == 0)
		tablespace_inotify_init0();
	else if (strcmp(cmd, "clear") == 0)
		tablespace_inotify_clear(name);
	else if (strcmp(cmd, "exit") == 0)
		tablespace_inotify_exit();
	else if (strcmp(cmd, "add") == 0)
		tablespace_inotify_add(name, TextDatumGetCString(PG_GETARG_TEXT_PP(2)));
	else if (strcmp(cmd, "checkout") == 0)
		tablespace_inotify_checkout();
	else
		ereport(ERROR, (errmsg("Unknown cmd=%s", cmd)));

	PG_RETURN_NULL();
}

PG_FUNCTION_INFO_V1(gp_tablespace_tmppath);
Datum
gp_tablespace_tmppath(PG_FUNCTION_ARGS)
{
	char tempdirpath[4096];
	Oid tblspcOid = PG_GETARG_OID(0);
	if (!OidIsValid(tblspcOid))
	{
		tblspcOid = GetSessionTempTableSpace();
		if (!OidIsValid(tblspcOid))
			tblspcOid = MyDatabaseTableSpace ? MyDatabaseTableSpace : DEFAULTTABLESPACE_OID;
	}
	if (tblspcOid == DEFAULTTABLESPACE_OID ||
		tblspcOid == GLOBALTABLESPACE_OID)
	{
		snprintf(tempdirpath, sizeof(tempdirpath), "base/%s",
				PG_TEMP_FILES_DIR);
	}
	else
	{
		snprintf(tempdirpath, sizeof(tempdirpath), "pg_tblspc/%u/%s/%s",
				tblspcOid, GP_TABLESPACE_VERSION_DIRECTORY, PG_TEMP_FILES_DIR);
	}
	PG_RETURN_TEXT_P(CStringGetTextDatum(tempdirpath));
}
