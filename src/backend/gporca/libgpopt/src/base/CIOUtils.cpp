//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2013 EMC Corp.
//
//	@filename:
//		CIOUtils.cpp
//
//	@doc:
//		Implementation of optimizer I/O utilities
//---------------------------------------------------------------------------

#include "gpos/base.h"
#include "gpos/task/CAutoSuspendAbort.h"
#include "gpos/task/CWorker.h"
#include "gpos/io/COstreamFile.h"

#include "gpopt/base/CIOUtils.h"

using namespace gpopt;



//---------------------------------------------------------------------------
//	@function:
//		CIOUtils::Dump
//
//	@doc:
//		Dump given string to output file
//
//---------------------------------------------------------------------------
void
CIOUtils::Dump(CHAR *file_name, CHAR *sz)
{
	CAutoSuspendAbort asa;

	const ULONG ulWrPerms = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	GPOS_TRY
	{
		CFileWriter fw;
		fw.Open(file_name, ulWrPerms);
		const BYTE *pb = reinterpret_cast<const BYTE *>(sz);
		ULONG_PTR ulpLength = (ULONG_PTR) clib::Strlen(sz);
		fw.Write(pb, ulpLength);
		fw.Close();
	}
	GPOS_CATCH_EX(ex)
	{
		// ignore exceptions during dumping
		GPOS_RESET_EX;
	}
	GPOS_CATCH_END;
}


// EOF
