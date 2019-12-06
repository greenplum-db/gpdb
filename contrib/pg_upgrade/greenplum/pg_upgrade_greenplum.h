#include "extra_cluster_info.h"
#include "old_tablespace_file_contents.h"


typedef struct ExtraClusterInfo
{
	int gp_dbid;
	OldTablespaceFileContents *old_tablespace_file_contents;
} GreenplumClusterInfo;
