//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2010 Greenplum, Inc.
//
//	@filename:
//		CStackDescriptor.cpp
//
//	@doc:
//		Implementation of interface class for execution stack tracing.
//---------------------------------------------------------------------------

#include "gpos/common/CStackDescriptor.h"

#include "gpos/string/CWString.h"
#include "gpos/task/IWorker.h"
#include "gpos/utils.h"

#define GPOS_STACK_DESCR_TRACE_BUF (4096)

using namespace gpos;

//---------------------------------------------------------------------------
//	@function:
//		CStackDescriptor::BackTrace
//
//	@doc:
//		Store current stack
//
//---------------------------------------------------------------------------
void
CStackDescriptor::BackTrace(ULONG top_frames_to_skip)
{
	// get base pointer of current frame
	ULONG_PTR current_frame;
	GPOS_GET_FRAME_POINTER(current_frame);

	// reset stack depth
	Reset();

	// pointer to next frame in stack
	void **next_frame = (void **) current_frame;

	// get stack start address
	ULONG_PTR stack_start = 0;
	IWorker *worker = IWorker::Self();
	if (nullptr == worker)
	{
		// no worker in stack, return immediately
		return;
	}

	// get address from worker
	stack_start = worker->GetStackStart();

	// consider the first GPOS_STACK_TRACE_DEPTH frames below worker object
	for (ULONG frame_counter = 0; frame_counter < GPOS_STACK_TRACE_DEPTH;
		 frame_counter++)
	{
		// check if the frame pointer is after stack start and before previous frame
		if ((ULONG_PTR) *next_frame > stack_start ||
			(ULONG_PTR) *next_frame < (ULONG_PTR) next_frame)
		{
			break;
		}

		// skip top frames
		if (0 < top_frames_to_skip)
		{
			top_frames_to_skip--;
		}
		else
		{
			// get return address (one above the base pointer)
			ULONG_PTR *frame_address = (ULONG_PTR *) (next_frame + 1);
			m_array_of_addresses[m_depth++] = (void *) *frame_address;
		}

		// move to next frame
		next_frame = (void **) *next_frame;
	}
}

//---------------------------------------------------------------------------
//	@function:
//		CStackDescriptor::AppendSymbolInfo
//
//	@doc:
//		Append formatted symbol description
//
//---------------------------------------------------------------------------
void
CStackDescriptor::AppendSymbolInfo(CWString *ws, CHAR *demangling_symbol_buffer,
								   SIZE_T size, const DL_INFO &symbol_info,
								   ULONG index) const
{
	const CHAR *symbol_name = demangling_symbol_buffer;

	// resolve symbol name
	if (symbol_info.dli_sname)
	{
		INT status = 0;
		symbol_name = symbol_info.dli_sname;

		// demangle C++ symbol
		CHAR *demangled_symbol = clib::Demangle(
			symbol_name, demangling_symbol_buffer, &size, &status);
		GPOS_ASSERT(size <= GPOS_STACK_SYMBOL_SIZE);

		if (0 == status)
		{
			// skip args and template symbol_info
			for (ULONG ul = 0; ul < size; ul++)
			{
				if ('(' == demangling_symbol_buffer[ul] ||
					'<' == demangling_symbol_buffer[ul])
				{
					demangling_symbol_buffer[ul] = '\0';
					break;
				}
			}

			symbol_name = demangled_symbol;
		}
	}
	else
	{
		symbol_name = "<symbol not found>";
	}

	// format symbol symbol_info
	ws->AppendFormat(
		GPOS_WSZ_LIT("%-4d 0x%016lx %s + %lu\n"),
		index + 1,	// frame no.
		(long unsigned int)
			m_array_of_addresses[index],  // current address in frame
		symbol_name,					  // symbol name
		(long unsigned int) m_array_of_addresses[index] -
			(ULONG_PTR) symbol_info.dli_saddr
		// offset from frame start
	);
}


//---------------------------------------------------------------------------
//	@function:
//		CStackDescriptor::AppendTrace
//
//	@doc:
//		Append trace of stored stack to string
//
//---------------------------------------------------------------------------
void
CStackDescriptor::AppendTrace(CWString *ws, ULONG depth) const
{
	GPOS_ASSERT(GPOS_STACK_TRACE_DEPTH >= m_depth &&
				"Stack exceeds maximum depth");

	// symbol symbol_info
	Dl_info symbol_info;


	// buffer for symbol demangling
	CHAR demangling_symbol_buffer[GPOS_STACK_SYMBOL_SIZE];

	// print symbol_info for frames in stack
	for (ULONG i = 0; i < m_depth && i < depth; i++)
	{
		// resolve address
		clib::Dladdr(m_array_of_addresses[i], &symbol_info);

		// get symbol description
		AppendSymbolInfo(ws, demangling_symbol_buffer, GPOS_STACK_SYMBOL_SIZE,
						 symbol_info, i);
	}
}


//---------------------------------------------------------------------------
//	@function:
//		CStackDescriptor::AppendTrace
//
//	@doc:
//		Append trace of stored stack to stream
//
//---------------------------------------------------------------------------
void
CStackDescriptor::AppendTrace(IOstream &os, ULONG depth) const
{
	WCHAR wsz[GPOS_STACK_DESCR_TRACE_BUF];
	CWStringStatic str(wsz, GPOS_ARRAY_SIZE(wsz));

	AppendTrace(&str, depth);
	os << str.GetBuffer();
}


//---------------------------------------------------------------------------
//	@function:
//		CStackDescriptor::HashValue
//
//	@doc:
//		Get hash value for stored stack
//
//---------------------------------------------------------------------------
ULONG
CStackDescriptor::HashValue() const
{
	GPOS_ASSERT(0 < m_depth && "No stack to hash");
	GPOS_ASSERT(GPOS_STACK_TRACE_DEPTH >= m_depth &&
				"Stack exceeds maximum depth");

	return gpos::HashByteArray((BYTE *) m_array_of_addresses,
							   m_depth * GPOS_SIZEOF(m_array_of_addresses[0]));
}

// EOF
