//---------------------------------------------------------------------------
//	Greenplum Database
//	Copyright (C) 2020 VMware, Inc.
//---------------------------------------------------------------------------
#ifndef GPDB_RelationWrapper_H
#define GPDB_RelationWrapper_H

#include <cstddef>

typedef struct RelationData *Relation;

namespace gpdb
{
/// \class
/// A transparent RAII wrapper for a pointer to a Postgres RelationData.
/// "Transparent" means that an object of this type can be used in most contexts
/// that expect a Relation. The main advantage of using this wrapper is that it
/// automatically closes the wrapper relation when exiting scope. So you no
/// longer have to write code like this:
/// \code
/// void RetrieveRel(Oid reloid) {
///     Relation rel = GetRelation(reloid);
///     if (IsPartialDist(rel)) {
///         CloseRelation(rel);
///         GPOS_RAISE(...);
///     }
///     try {
///         do_stuff();
///         CloseRelation(rel);
///     catch (...) {
///         CloseRelation(rel);
///         GPOS_RETHROW(...);
///     }
/// }
/// \endcode
/// and instead you can write this:
/// \code
/// void RetrieveRel(Oid reloid) {
///     auto rel = GetRelation(reloid);
///     if (IsPartialDist(rel)) {
///         GPOS_RAISE(...);
///     }
///     do_stuff();
/// }
/// \endcode
class RelationWrapper
{
public:
	RelationWrapper(RelationWrapper const &) = delete;
	RelationWrapper(RelationWrapper &&r) : m_relation(r.m_relation)
	{
		r.m_relation = nullptr;
	};

	explicit RelationWrapper(Relation relation) : m_relation(relation)
	{
	}

	/// the following two operators allow use in typical conditionals of the
	/// form.
	///
	/// \code if (rel == nullptr) return; \endcode or
	/// \code if (rel != nullptr) do_stuff(rel); \endcode
	///
	/// They are currently unused because the preferred form is
	///
	/// \code if (rel) ... \endcode or
	/// \code if (!rel) ... \endcode
	bool operator==(std::nullptr_t) const
	{
		return m_relation == nullptr;
	}

	bool operator!=(std::nullptr_t) const
	{
		return m_relation != nullptr;
	}

	/// allows use in typical conditionals of the form
	///
	/// \code if (rel) { do_stuff(rel); } \endcode or
	/// \code if (!rel) return; \endcode
	explicit operator bool() const
	{
		return m_relation != nullptr;
	}

	// behave like a raw pointer on arrow
	Relation
	operator->() const
	{
		return m_relation;
	}

	// FIXME: this eases the transition, but an implicit cast is an antipattern
	// once we're done migrating, replace with something like get()
	operator Relation() const
	{
		return m_relation;
	}

	/// Explicitly close the underlying relation early. This is not usually
	/// necessary unless there is significant amount of time between the point
	/// of close and the end-of-scope
	void Close();

	~RelationWrapper() noexcept(false);

private:
	Relation m_relation = nullptr;
};
}  // namespace gpdb
#endif	// GPDB_RelationWrapper_H
