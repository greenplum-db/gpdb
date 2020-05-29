
//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2019 Pivotal
//
//---------------------------------------------------------------------------
#ifndef GPOS_CLink_H
#define GPOS_CLink_H


namespace gpos
{


	// Generic link to be embedded in all classes before they can use
	// allocation-less lists, e.g. in synchronized hashtables etc.
	struct SLink
	{

		private:

			SLink(const SLink&) = delete;

		public:

			// link forward/backward
			void *m_next;
			void *m_prev;

			// ctor
			SLink()
				:
				m_next(NULL),
				m_prev(NULL)
			{}
	};

}

#endif // !GPOS_CLink_H

// EOF

