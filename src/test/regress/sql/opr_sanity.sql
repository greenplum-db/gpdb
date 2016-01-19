--
-- OPR_SANITY
-- Sanity checks for common errors in making operator/procedure system tables:
-- pg_operator, pg_proc, pg_cast, pg_aggregate, pg_am,
-- pg_amop, pg_amproc, pg_opclass, pg_opfamily.
--
-- None of the SELECTs here should ever find any matching entries,
-- so the expected output is easy to maintain ;-).
-- A test failure indicates someone messed up an entry in the system tables.
--
-- NB: we assume the oidjoins test will have caught any dangling links,
-- that is OID or REGPROC fields that are not zero and do not match some
-- row in the linked-to table.  However, if we want to enforce that a link
-- field can't be 0, we have to check it here.
--
-- NB: run this test earlier than the create_operator test, because
-- that test creates some bogus operators...


-- Helper functions to deal with cases where binary-coercible matches are
-- allowed.

-- This should match IsBinaryCoercible() in parse_coerce.c.
create function binary_coercible(oid, oid) returns bool as $$
SELECT ($1 = $2) OR
 EXISTS(select 1 from pg_catalog.pg_cast where
        castsource = $1 and casttarget = $2 and
        castfunc = 0 and castcontext = 'i') OR
 ($2 = 'pg_catalog.anyarray'::pg_catalog.regtype AND
  EXISTS(select 1 from pg_catalog.pg_type where
         oid = $1 and typelem != 0 and typlen = -1))
$$ language sql strict stable READS SQL DATA;

-- This one ignores castcontext, so it considers only physical equivalence
-- and not whether the coercion can be invoked implicitly.
create function physically_coercible(oid, oid) returns bool as $$
SELECT ($1 = $2) OR
 EXISTS(select 1 from pg_catalog.pg_cast where
        castsource = $1 and casttarget = $2 and
        castfunc = 0) OR
 ($2 = 'pg_catalog.anyarray'::pg_catalog.regtype AND
  EXISTS(select 1 from pg_catalog.pg_type where
         oid = $1 and typelem != 0 and typlen = -1))
$$ language sql strict stable READS SQL DATA;

-- **************** pg_proc ****************

-- Look for illegal values in pg_proc fields.

SELECT p1.oid, p1.proname
FROM pg_proc as p1
WHERE p1.prolang = 0 OR p1.prorettype = 0 OR
       p1.pronargs < 0 OR
       p1.pronargdefaults < 0 OR
       p1.pronargdefaults > p1.pronargs OR
       array_lower(p1.proargtypes, 1) != 0 OR
       array_upper(p1.proargtypes, 1) != p1.pronargs-1 OR
       0::oid = ANY (p1.proargtypes);

-- pronargdefaults should be 0 iff proargdefaults is null
SELECT p1.oid, p1.proname
FROM pg_proc AS p1
WHERE (pronargdefaults <> 0) != (proargdefaults IS NOT NULL);

-- Look for conflicting proc definitions (same names and input datatypes).
-- (This test should be dead code now that we have the unique index
-- pg_proc_proname_args_nsp_index, but I'll leave it in anyway.)

SELECT p1.oid, p1.proname, p2.oid, p2.proname
FROM pg_proc AS p1, pg_proc AS p2
WHERE p1.oid != p2.oid AND
    p1.proname = p2.proname AND
    p1.pronargs = p2.pronargs AND
    p1.proargtypes = p2.proargtypes;

-- Considering only built-in procs (prolang = 12), look for multiple uses
-- of the same internal function (ie, matching prosrc fields).  It's OK to
-- have several entries with different pronames for the same internal function,
-- but conflicts in the number of arguments (except in the case of window 
-- functions) and other critical items should be complained of.  (We don't 
-- check data types here; see next query.) 
--
-- Note: ignore aggregate functions here, since they all point to the same dummy
-- built-in function.
--
-- Note: ignoring gp_deprecated here since the function ignores its parameters
-- always errors and never returns a value.

SELECT p1.oid, p1.proname, p2.oid, p2.proname
FROM pg_proc AS p1, pg_proc AS p2
WHERE p1.oid < p2.oid AND
    p1.prosrc = p2.prosrc AND
	p1.prosrc NOT IN ('gp_deprecated') AND
    p1.prolang = 12 AND p2.prolang = 12 AND
    (p1.proisagg = false OR p2.proisagg = false) AND
    (p1.proiswin = false OR p1.proiswin = false) AND
    (p1.prolang != p2.prolang OR
     p1.proisagg != p2.proisagg OR
     p1.proiswin != p2.proiswin OR
     p1.prosecdef != p2.prosecdef OR
     p1.proisstrict != p2.proisstrict OR
     p1.proretset != p2.proretset OR
     p1.provolatile != p2.provolatile OR
     (p1.pronargs != p2.pronargs AND p1.oid 
	  NOT IN (select winfunc from pg_window)));

-- Look for uses of different type OIDs in the argument/result type fields
-- for different aliases of the same built-in function.
-- This indicates that the types are being presumed to be binary-equivalent,
-- or that the built-in function is prepared to deal with different types.
-- That's not wrong, necessarily, but we make lists of all the types being
-- so treated.  Note that the expected output of this part of the test will
-- need to be modified whenever new pairs of types are made binary-equivalent,
-- or when new polymorphic built-in functions are added!
--
-- Note: ignore aggregate functions here, since they all point to the same
-- dummy built-in function.
--
-- Note: ignoring gp_deprecated here since the function ignores its parameters
-- always errors and never returns a value.

SELECT DISTINCT p1.prorettype::regtype, p2.prorettype::regtype
FROM pg_proc AS p1, pg_proc AS p2
WHERE p1.oid != p2.oid AND
    p1.prosrc = p2.prosrc AND
	p1.prosrc NOT IN ('gp_deprecated') AND
    p1.prolang = 12 AND p2.prolang = 12 AND
    NOT p1.proisagg AND NOT p2.proisagg AND
    NOT p1.proiswin AND NOT p2.proiswin AND
    (p1.prorettype < p2.prorettype);

SELECT DISTINCT p1.proargtypes[0]::regtype, p2.proargtypes[0]::regtype
FROM pg_proc AS p1, pg_proc AS p2
WHERE p1.oid != p2.oid AND
    p1.prosrc = p2.prosrc AND
	p1.prosrc NOT IN ('gp_deprecated') AND
    p1.prolang = 12 AND p2.prolang = 12 AND
    NOT p1.proisagg AND NOT p2.proisagg AND
    NOT p1.proiswin AND NOT p2.proiswin AND
    (p1.proargtypes[0] < p2.proargtypes[0]);

SELECT DISTINCT p1.proargtypes[1]::regtype, p2.proargtypes[1]::regtype
FROM pg_proc AS p1, pg_proc AS p2
WHERE p1.oid != p2.oid AND
    p1.prosrc = p2.prosrc AND
	p1.prosrc NOT IN ('gp_deprecated') AND
    p1.prolang = 12 AND p2.prolang = 12 AND
    NOT p1.proisagg AND NOT p2.proisagg AND
    NOT p1.proiswin AND NOT p2.proiswin AND
    (p1.proargtypes[1] < p2.proargtypes[1]);

SELECT DISTINCT p1.proargtypes[2]::regtype, p2.proargtypes[2]::regtype
FROM pg_proc AS p1, pg_proc AS p2
WHERE p1.oid != p2.oid AND
    p1.prosrc = p2.prosrc AND
	p1.prosrc NOT IN ('gp_deprecated') AND
    p1.prolang = 12 AND p2.prolang = 12 AND
    NOT p1.proisagg AND NOT p2.proisagg AND
    NOT p1.proiswin AND NOT p2.proiswin AND
    (p1.proargtypes[2] < p2.proargtypes[2]);

SELECT DISTINCT p1.proargtypes[3]::regtype, p2.proargtypes[3]::regtype
FROM pg_proc AS p1, pg_proc AS p2
WHERE p1.oid != p2.oid AND
    p1.prosrc = p2.prosrc AND
	p1.prosrc NOT IN ('gp_deprecated') AND
    p1.prolang = 12 AND p2.prolang = 12 AND
    NOT p1.proisagg AND NOT p2.proisagg AND
    NOT p1.proiswin AND NOT p2.proiswin AND
    (p1.proargtypes[3] < p2.proargtypes[3]);

SELECT DISTINCT p1.proargtypes[4]::regtype, p2.proargtypes[4]::regtype
FROM pg_proc AS p1, pg_proc AS p2
WHERE p1.oid != p2.oid AND
    p1.prosrc = p2.prosrc AND
	p1.prosrc NOT IN ('gp_deprecated') AND
    p1.prolang = 12 AND p2.prolang = 12 AND
    NOT p1.proisagg AND NOT p2.proisagg AND
    NOT p1.proiswin AND NOT p2.proiswin AND
    (p1.proargtypes[4] < p2.proargtypes[4]);

SELECT DISTINCT p1.proargtypes[5]::regtype, p2.proargtypes[5]::regtype
FROM pg_proc AS p1, pg_proc AS p2
WHERE p1.oid != p2.oid AND
    p1.prosrc = p2.prosrc AND
	p1.prosrc NOT IN ('gp_deprecated') AND
    p1.prolang = 12 AND p2.prolang = 12 AND
    NOT p1.proisagg AND NOT p2.proisagg AND
    NOT p1.proiswin AND NOT p2.proiswin AND
    (p1.proargtypes[5] < p2.proargtypes[5]);

SELECT DISTINCT p1.proargtypes[6]::regtype, p2.proargtypes[6]::regtype
FROM pg_proc AS p1, pg_proc AS p2
WHERE p1.oid != p2.oid AND
    p1.prosrc = p2.prosrc AND
	p1.prosrc NOT IN ('gp_deprecated') AND
    p1.prolang = 12 AND p2.prolang = 12 AND
    NOT p1.proisagg AND NOT p2.proisagg AND
    NOT p1.proiswin AND NOT p2.proiswin AND
    (p1.proargtypes[6] < p2.proargtypes[6]);

SELECT DISTINCT p1.proargtypes[7]::regtype, p2.proargtypes[7]::regtype
FROM pg_proc AS p1, pg_proc AS p2
WHERE p1.oid != p2.oid AND
    p1.prosrc = p2.prosrc AND
	p1.prosrc NOT IN ('gp_deprecated') AND
    p1.prolang = 12 AND p2.prolang = 12 AND
    NOT p1.proisagg AND NOT p2.proisagg AND
    NOT p1.proiswin AND NOT p2.proiswin AND
    (p1.proargtypes[7] < p2.proargtypes[7]);

-- The checks above only extend to the first 8 parameters, list any
-- functions not checked.
SELECT proname, pronargs 
FROM pg_proc 
WHERE pronargs > 8 and prolang = 12 AND
	prosrc NOT IN ('gp_deprecated') AND
	proname NOT IN ('gp_add_persistent_filespace_node_entry',
					'gp_add_persistent_relation_node_entry',
					'gp_update_persistent_filespace_node_entry',
					'gp_update_persistent_relation_node_entry',
					'gp_add_persistent_database_node_entry',
					'gp_add_persistent_tablespace_node_entry',
					'gp_update_persistent_database_node_entry',
					'gp_update_persistent_tablespace_node_entry') AND
    NOT proisagg AND 
    NOT proiswin;


-- Look for functions that return type "internal" and do not have any
-- "internal" argument.  Such a function would be a security hole since
-- it might be used to call an internal function from an SQL command.
-- As of 7.3 this query should find only internal_in.

SELECT p1.oid, p1.proname
FROM pg_proc as p1
WHERE p1.prorettype = 'internal'::regtype AND NOT
    'internal'::regtype = ANY (p1.proargtypes);


-- **************** pg_cast ****************

-- Catch bogus values in pg_cast columns (other than cases detected by
-- oidjoins test).

SELECT castsource::regtype, casttarget::regtype, castfunc::regproc, castcontext
FROM pg_cast c
WHERE castsource = 0 OR casttarget = 0 OR castcontext NOT IN ('e', 'a', 'i');

-- Look for casts to/from the same type that aren't length coercion functions.
-- (We assume they are length coercions if they take multiple arguments.)
-- Such entries are not necessarily harmful, but they are useless.

SELECT castsource::regtype, casttarget::regtype, castfunc::regproc, castcontext
FROM pg_cast c
WHERE castsource = casttarget AND castfunc = 0;

SELECT castsource::regtype, casttarget::regtype, castfunc::regproc, castcontext
FROM pg_cast c, pg_proc p
WHERE c.castfunc = p.oid AND p.pronargs < 2 AND castsource = casttarget;

-- Look for cast functions that don't have the right signature.  The
-- argument and result types in pg_proc must be the same as, or binary
-- compatible with, what it says in pg_cast.
-- As a special case, we allow casts from CHAR(n) that use functions
-- declared to take TEXT.  This does not pass the binary-coercibility test
-- because CHAR(n)-to-TEXT normally invokes rtrim().  However, the results
-- are the same, so long as the function is one that ignores trailing blanks.

SELECT castsource::regtype, casttarget::regtype, castfunc::regproc, castcontext
FROM pg_cast c, pg_proc p
WHERE c.castfunc = p.oid AND
    (p.pronargs < 1 OR p.pronargs > 3
     OR NOT (binary_coercible(c.castsource, p.proargtypes[0])
             OR (c.castsource = 'character'::regtype AND
                 p.proargtypes[0] = 'text'::regtype))
     OR NOT binary_coercible(p.prorettype, c.casttarget));

SELECT castsource::regtype, casttarget::regtype, castfunc::regproc, castcontext
FROM pg_cast c, pg_proc p
WHERE c.castfunc = p.oid AND
    ((p.pronargs > 1 AND p.proargtypes[1] != 'int4'::regtype) OR
     (p.pronargs > 2 AND p.proargtypes[2] != 'bool'::regtype));

-- Look for binary compatible casts that do not have the reverse
-- direction registered as well, or where the reverse direction is not
-- also binary compatible.  This is legal, but usually not intended.

-- As of 7.4, this finds the casts from text and varchar to bpchar, because
-- those are binary-compatible while the reverse way goes through rtrim().

-- As of 8.2, this finds the cast from cidr to inet, because that is a
-- trivial binary coercion while the other way goes through inet_to_cidr().

-- As of 8.3, this finds casts from xml to text, varchar, and bpchar,
-- because the other direction has to go through xmlparse().

SELECT *
FROM pg_cast c
WHERE c.castfunc = 0 AND
    NOT EXISTS (SELECT 1 FROM pg_cast k
                WHERE k.castfunc = 0 AND
                    k.castsource = c.casttarget AND
                    k.casttarget = c.castsource);

-- **************** pg_operator ****************

-- Look for illegal values in pg_operator fields.

SELECT p1.oid, p1.oprname
FROM pg_operator as p1
WHERE (p1.oprkind != 'b' AND p1.oprkind != 'l' AND p1.oprkind != 'r') OR
    p1.oprresult = 0 OR p1.oprcode = 0;

-- Look for missing or unwanted operand types

SELECT p1.oid, p1.oprname
FROM pg_operator as p1
WHERE (p1.oprleft = 0 and p1.oprkind != 'l') OR
    (p1.oprleft != 0 and p1.oprkind = 'l') OR
    (p1.oprright = 0 and p1.oprkind != 'r') OR
    (p1.oprright != 0 and p1.oprkind = 'r');

-- Look for conflicting operator definitions (same names and input datatypes).

SELECT p1.oid, p1.oprcode, p2.oid, p2.oprcode
FROM pg_operator AS p1, pg_operator AS p2
WHERE p1.oid != p2.oid AND
    p1.oprname = p2.oprname AND
    p1.oprkind = p2.oprkind AND
    p1.oprleft = p2.oprleft AND
    p1.oprright = p2.oprright;

-- Look for commutative operators that don't commute.
-- DEFINITIONAL NOTE: If A.oprcom = B, then x A y has the same result as y B x.
-- We expect that B will always say that B.oprcom = A as well; that's not
-- inherently essential, but it would be inefficient not to mark it so.

SELECT p1.oid, p1.oprcode, p2.oid, p2.oprcode
FROM pg_operator AS p1, pg_operator AS p2
WHERE p1.oprcom = p2.oid AND
    (p1.oprkind != 'b' OR
     p1.oprleft != p2.oprright OR
     p1.oprright != p2.oprleft OR
     p1.oprresult != p2.oprresult OR
     p1.oid != p2.oprcom);

-- Look for negatory operators that don't agree.
-- DEFINITIONAL NOTE: If A.oprnegate = B, then both A and B must yield
-- boolean results, and (x A y) == ! (x B y), or the equivalent for
-- single-operand operators.
-- We expect that B will always say that B.oprnegate = A as well; that's not
-- inherently essential, but it would be inefficient not to mark it so.
-- Also, A and B had better not be the same operator.

SELECT p1.oid, p1.oprcode, p2.oid, p2.oprcode
FROM pg_operator AS p1, pg_operator AS p2
WHERE p1.oprnegate = p2.oid AND
    (p1.oprkind != p2.oprkind OR
     p1.oprleft != p2.oprleft OR
     p1.oprright != p2.oprright OR
     p1.oprresult != 'bool'::regtype OR
     p2.oprresult != 'bool'::regtype OR
     p1.oid != p2.oprnegate OR
     p1.oid = p2.oid);

-- A mergejoinable or hashjoinable operator must be binary, must return
-- boolean, and must have a commutator (itself, unless it's a cross-type
-- operator).

SELECT p1.oid, p1.oprname FROM pg_operator AS p1
WHERE (p1.oprcanmerge OR p1.oprcanhash) AND NOT
    (p1.oprkind = 'b' AND p1.oprresult = 'bool'::regtype AND p1.oprcom != 0);

-- Mergejoinable operators should appear as equality members of btree index
-- opfamilies.

SELECT p1.oid, p1.oprname
FROM pg_operator AS p1
WHERE p1.oprcanmerge AND NOT EXISTS
  (SELECT 1 FROM pg_amop
   WHERE amopmethod = (SELECT oid FROM pg_am WHERE amname = 'btree') AND
         amopopr = p1.oid AND amopstrategy = 3);

-- And the converse.

SELECT p1.oid, p1.oprname, p.amopfamily
FROM pg_operator AS p1, pg_amop p
WHERE amopopr = p1.oid
  AND amopmethod = (SELECT oid FROM pg_am WHERE amname = 'btree')
  AND amopstrategy = 3
  AND NOT p1.oprcanmerge;

-- Hashable operators should appear as members of hash index opfamilies.

SELECT p1.oid, p1.oprname
FROM pg_operator AS p1
WHERE p1.oprcanhash AND NOT EXISTS
  (SELECT 1 FROM pg_amop
   WHERE amopmethod = (SELECT oid FROM pg_am WHERE amname = 'hash') AND
         amopopr = p1.oid AND amopstrategy = 1);

-- And the converse.

SELECT p1.oid, p1.oprname, p.amopfamily
FROM pg_operator AS p1, pg_amop p
WHERE amopopr = p1.oid
  AND amopmethod = (SELECT oid FROM pg_am WHERE amname = 'hash')
  AND NOT p1.oprcanhash;

-- Check that each operator defined in pg_operator matches its oprcode entry
-- in pg_proc.  Easiest to do this separately for each oprkind.

SELECT p1.oid, p1.oprname, p2.oid, p2.proname
FROM pg_operator AS p1, pg_proc AS p2
WHERE p1.oprcode = p2.oid AND
    p1.oprkind = 'b' AND
    (p2.pronargs != 2
     OR NOT binary_coercible(p2.prorettype, p1.oprresult)
     OR NOT binary_coercible(p1.oprleft, p2.proargtypes[0])
     OR NOT binary_coercible(p1.oprright, p2.proargtypes[1]));

SELECT p1.oid, p1.oprname, p2.oid, p2.proname
FROM pg_operator AS p1, pg_proc AS p2
WHERE p1.oprcode = p2.oid AND
    p1.oprkind = 'l' AND
    (p2.pronargs != 1
     OR NOT binary_coercible(p2.prorettype, p1.oprresult)
     OR NOT binary_coercible(p1.oprright, p2.proargtypes[0])
     OR p1.oprleft != 0);

SELECT p1.oid, p1.oprname, p2.oid, p2.proname
FROM pg_operator AS p1, pg_proc AS p2
WHERE p1.oprcode = p2.oid AND
    p1.oprkind = 'r' AND
    (p2.pronargs != 1
     OR NOT binary_coercible(p2.prorettype, p1.oprresult)
     OR NOT binary_coercible(p1.oprleft, p2.proargtypes[0])
     OR p1.oprright != 0);

-- If the operator is mergejoinable or hashjoinable, its underlying function
-- should not be volatile.

SELECT p1.oid, p1.oprname, p2.oid, p2.proname
FROM pg_operator AS p1, pg_proc AS p2
WHERE p1.oprcode = p2.oid AND
    (p1.oprcanmerge OR p1.oprcanhash) AND
    p2.provolatile = 'v';

-- If oprrest is set, the operator must return boolean,
-- and it must link to a proc with the right signature
-- to be a restriction selectivity estimator.
-- The proc signature we want is: float8 proc(internal, oid, internal, int4)

SELECT p1.oid, p1.oprname, p2.oid, p2.proname
FROM pg_operator AS p1, pg_proc AS p2
WHERE p1.oprrest = p2.oid AND
    (p1.oprresult != 'bool'::regtype OR
     p2.prorettype != 'float8'::regtype OR p2.proretset OR
     p2.pronargs != 4 OR
     p2.proargtypes[0] != 'internal'::regtype OR
     p2.proargtypes[1] != 'oid'::regtype OR
     p2.proargtypes[2] != 'internal'::regtype OR
     p2.proargtypes[3] != 'int4'::regtype);

-- If oprjoin is set, the operator must be a binary boolean op,
-- and it must link to a proc with the right signature
-- to be a join selectivity estimator.
-- The proc signature we want is: float8 proc(internal, oid, internal, int2)

SELECT p1.oid, p1.oprname, p2.oid, p2.proname
FROM pg_operator AS p1, pg_proc AS p2
WHERE p1.oprjoin = p2.oid AND
    (p1.oprkind != 'b' OR p1.oprresult != 'bool'::regtype OR
     p2.prorettype != 'float8'::regtype OR p2.proretset OR
     p2.pronargs != 4 OR
     p2.proargtypes[0] != 'internal'::regtype OR
     p2.proargtypes[1] != 'oid'::regtype OR
     p2.proargtypes[2] != 'internal'::regtype OR
     p2.proargtypes[3] != 'int2'::regtype);

-- **************** pg_aggregate ****************

-- Look for illegal values in pg_aggregate fields.

SELECT ctid, aggfnoid::oid
FROM pg_aggregate as p1
WHERE aggfnoid = 0 OR aggtransfn = 0 OR aggtranstype = 0;

-- Make sure the matching pg_proc entry is sensible, too.

SELECT a.aggfnoid::oid, p.proname
FROM pg_aggregate as a, pg_proc as p
WHERE a.aggfnoid = p.oid AND
    (NOT p.proisagg OR p.proretset);

-- Make sure there are no proisagg pg_proc entries without matches.

SELECT oid, proname
FROM pg_proc as p
WHERE p.proisagg AND
    NOT EXISTS (SELECT 1 FROM pg_aggregate a WHERE a.aggfnoid = p.oid);

-- If there is no finalfn then the output type must be the transtype.

SELECT a.aggfnoid::oid, p.proname
FROM pg_aggregate as a, pg_proc as p
WHERE a.aggfnoid = p.oid AND
    a.aggfinalfn = 0 AND p.prorettype != a.aggtranstype;

-- Cross-check transfn against its entry in pg_proc.
-- NOTE: use physically_coercible here, not binary_coercible, because
-- max and min on abstime are implemented using int4larger/int4smaller.
SELECT a.aggfnoid::oid, p.proname, ptr.oid, ptr.proname
FROM pg_aggregate AS a, pg_proc AS p, pg_proc AS ptr
WHERE a.aggfnoid = p.oid AND
    a.aggtransfn = ptr.oid AND
    (ptr.proretset
     OR NOT (ptr.pronargs = p.pronargs + 1)
     OR NOT physically_coercible(ptr.prorettype, a.aggtranstype)
     OR NOT physically_coercible(a.aggtranstype, ptr.proargtypes[0])
     OR (p.pronargs > 0 AND
         NOT physically_coercible(p.proargtypes[0], ptr.proargtypes[1]))
     OR (p.pronargs > 1 AND
         NOT physically_coercible(p.proargtypes[1], ptr.proargtypes[2]))
     OR (p.pronargs > 2 AND
         NOT physically_coercible(p.proargtypes[2], ptr.proargtypes[3]))
     -- we could carry the check further, but that's enough for now
    );

-- Cross-check finalfn (if present) against its entry in pg_proc.

SELECT a.aggfnoid::oid, p.proname, pfn.oid, pfn.proname
FROM pg_aggregate AS a, pg_proc AS p, pg_proc AS pfn
WHERE a.aggfnoid = p.oid AND
    a.aggfinalfn = pfn.oid AND
    (pfn.proretset
     OR NOT binary_coercible(pfn.prorettype, p.prorettype)
     OR pfn.pronargs != 1
     OR NOT binary_coercible(a.aggtranstype, pfn.proargtypes[0]));

-- If transfn is strict then either initval should be non-NULL, or
-- input type should match transtype so that the first non-null input
-- can be assigned as the state value.

SELECT a.aggfnoid::oid, p.proname, ptr.oid, ptr.proname
FROM pg_aggregate AS a, pg_proc AS p, pg_proc AS ptr
WHERE a.aggfnoid = p.oid AND
    a.aggtransfn = ptr.oid AND ptr.proisstrict AND
    a.agginitval IS NULL AND
    NOT binary_coercible(p.proargtypes[0], a.aggtranstype);

-- Cross-check aggsortop (if present) against pg_operator.
-- We expect to find only "<" for "min" and ">" for "max".

SELECT DISTINCT proname, oprname
FROM pg_operator AS o, pg_aggregate AS a, pg_proc AS p
WHERE a.aggfnoid = p.oid AND a.aggsortop = o.oid
ORDER BY 1;

-- Check datatypes match

SELECT a.aggfnoid::oid, o.oid
FROM pg_operator AS o, pg_aggregate AS a, pg_proc AS p
WHERE a.aggfnoid = p.oid AND a.aggsortop = o.oid AND
    (oprkind != 'b' OR oprresult != 'boolean'::regtype
     OR oprleft != p.proargtypes[0] OR oprright != p.proargtypes[0]);

-- Check operator is a suitable btree opfamily member

SELECT a.aggfnoid::oid, o.oid
FROM pg_operator AS o, pg_aggregate AS a, pg_proc AS p
WHERE a.aggfnoid = p.oid AND a.aggsortop = o.oid AND
    NOT EXISTS(SELECT 1 FROM pg_amop
               WHERE amopmethod = (SELECT oid FROM pg_am WHERE amname = 'btree')
                     AND amopopr = o.oid
                     AND amoplefttype = o.oprleft
                     AND amoprighttype = o.oprright);

-- Check correspondence of btree strategies and names

SELECT DISTINCT proname, oprname, amopstrategy
FROM pg_operator AS o, pg_aggregate AS a, pg_proc AS p,
     pg_amop as ao
WHERE a.aggfnoid = p.oid AND a.aggsortop = o.oid AND
    amopopr = o.oid AND
    amopmethod = (SELECT oid FROM pg_am WHERE amname = 'btree')
ORDER BY 1, 2;

-- **************** pg_opfamily ****************

-- Look for illegal values in pg_opfamily fields

SELECT p1.oid
FROM pg_opfamily as p1
WHERE p1.opfmethod = 0 OR p1.opfnamespace = 0;

-- **************** pg_opclass ****************

-- Look for illegal values in pg_opclass fields

SELECT p1.oid
FROM pg_opclass AS p1
WHERE p1.opcmethod = 0 OR p1.opcnamespace = 0 OR p1.opcfamily = 0
    OR p1.opcintype = 0;

-- opcmethod must match owning opfamily's opfmethod

SELECT p1.oid, p2.oid
FROM pg_opclass AS p1, pg_opfamily AS p2
WHERE p1.opcfamily = p2.oid AND p1.opcmethod != p2.opfmethod;

-- There should not be multiple entries in pg_opclass with opcdefault true
-- and the same opcmethod/opcintype combination.

SELECT p1.oid, p2.oid
FROM pg_opclass AS p1, pg_opclass AS p2
WHERE p1.oid != p2.oid AND
    p1.opcmethod = p2.opcmethod AND p1.opcintype = p2.opcintype AND
    p1.opcdefault AND p2.opcdefault;

-- **************** pg_amop ****************

-- Look for illegal values in pg_amop fields

SELECT p1.amopfamily, p1.amopstrategy
FROM pg_amop as p1
WHERE p1.amopfamily = 0 OR p1.amoplefttype = 0 OR p1.amoprighttype = 0
    OR p1.amopopr = 0 OR p1.amopmethod = 0 OR p1.amopstrategy < 1;

-- amoplefttype/amoprighttype must match the operator

SELECT p1.oid, p2.oid
FROM pg_amop AS p1, pg_operator AS p2
WHERE p1.amopopr = p2.oid AND NOT
    (p1.amoplefttype = p2.oprleft AND p1.amoprighttype = p2.oprright);

-- amopmethod must match owning opfamily's opfmethod

SELECT p1.oid, p2.oid
FROM pg_amop AS p1, pg_opfamily AS p2
WHERE p1.amopfamily = p2.oid AND p1.amopmethod != p2.opfmethod;

-- Cross-check amopstrategy index against parent AM

SELECT p1.amopfamily, p1.amopopr, p2.oid, p2.amname
FROM pg_amop AS p1, pg_am AS p2
WHERE p1.amopmethod = p2.oid AND
    p1.amopstrategy > p2.amstrategies AND p2.amstrategies <> 0;

-- Detect missing pg_amop entries: should have as many strategy operators
-- as AM expects for each datatype combination supported by the opfamily.
-- We can't check this for AMs with variable strategy sets.

SELECT p1.amname, p2.amoplefttype, p2.amoprighttype
FROM pg_am AS p1, pg_amop AS p2
WHERE p2.amopmethod = p1.oid AND
    p1.amstrategies <> 0 AND
    p1.amstrategies != (SELECT count(*) FROM pg_amop AS p3
                        WHERE p3.amopfamily = p2.amopfamily AND
                              p3.amoplefttype = p2.amoplefttype AND
                              p3.amoprighttype = p2.amoprighttype);

-- Check that amopopr points at a reasonable-looking operator, ie a binary
-- operator yielding boolean.

SELECT p1.amopfamily, p1.amopopr, p2.oid, p2.oprname
FROM pg_amop AS p1, pg_operator AS p2
WHERE p1.amopopr = p2.oid AND
    (p2.oprkind != 'b' OR p2.oprresult != 'bool'::regtype);

-- Make a list of all the distinct operator names being used in particular
-- strategy slots.  This is a bit hokey, since the list might need to change
-- in future releases, but it's an effective way of spotting mistakes such as
-- swapping two operators within a family.

SELECT DISTINCT amopmethod, amopstrategy, oprname
FROM pg_amop p1 LEFT JOIN pg_operator p2 ON amopopr = p2.oid
ORDER BY 1, 2, 3;

-- Check that all operators linked to by opclass entries have selectivity
-- estimators.  This is not absolutely required, but it seems a reasonable
-- thing to insist on for all standard datatypes.

SELECT p1.amopfamily, p1.amopopr, p2.oid, p2.oprname
FROM pg_amop AS p1, pg_operator AS p2
WHERE p1.amopopr = p2.oid AND
    (p2.oprrest = 0 OR p2.oprjoin = 0);

-- Check that each opclass in an opfamily has associated operators, that is
-- ones whose oprleft matches opcintype (possibly by coercion).

SELECT p1.opcname, p1.opcfamily
FROM pg_opclass AS p1
WHERE NOT EXISTS(SELECT 1 FROM pg_amop AS p2
                 WHERE p2.amopfamily = p1.opcfamily
                   AND binary_coercible(p1.opcintype, p2.amoplefttype));

-- Operators that are primary members of opclasses must be immutable (else
-- it suggests that the index ordering isn't fixed).  Operators that are
-- cross-type members need only be stable, since they are just shorthands
-- for index probe queries.

SELECT p1.amopfamily, p1.amopopr, p2.oprname, p3.prosrc
FROM pg_amop AS p1, pg_operator AS p2, pg_proc AS p3
WHERE p1.amopopr = p2.oid AND p2.oprcode = p3.oid AND
    p1.amoplefttype = p1.amoprighttype AND
    p3.provolatile != 'i';

SELECT p1.amopfamily, p1.amopopr, p2.oprname, p3.prosrc
FROM pg_amop AS p1, pg_operator AS p2, pg_proc AS p3
WHERE p1.amopopr = p2.oid AND p2.oprcode = p3.oid AND
    p1.amoplefttype != p1.amoprighttype AND
    p3.provolatile = 'v';

-- Multiple-datatype btree opclasses should provide closed sets of equality
-- operators; that is if you provide int2 = int4 and int4 = int8 then you
-- must also provide int2 = int8 (and commutators of all these).  This is
-- necessary because the planner tries to deduce additional qual clauses from
-- transitivity of mergejoinable operators.  If there are clauses
-- int2var = int4var and int4var = int8var, the planner will deduce
-- int2var = int8var ... and it had better have a way to represent it.

-- check commutative closure
SELECT p1.amoplefttype, p1.amoprighttype
FROM pg_amop AS p1
WHERE p1.amopmethod = (SELECT oid FROM pg_am WHERE amname = 'btree') AND
    p1.amopstrategy = 3 AND
    p1.amoplefttype != p1.amoprighttype AND
    NOT EXISTS(SELECT 1 FROM pg_amop p2 WHERE
                 p2.amopfamily = p1.amopfamily AND
                 p2.amoplefttype = p1.amoprighttype AND
                 p2.amoprighttype = p1.amoplefttype AND
                 p2.amopstrategy = 3);

-- check transitive closure
SELECT p1.amoplefttype, p1.amoprighttype, p2.amoprighttype
FROM pg_amop AS p1, pg_amop AS p2
WHERE p1.amopfamily = p2.amopfamily AND
    p1.amoprighttype = p2.amoplefttype AND
    p1.amopmethod = (SELECT oid FROM pg_am WHERE amname = 'btree') AND
    p2.amopmethod = (SELECT oid FROM pg_am WHERE amname = 'btree') AND
    p1.amopstrategy = 3 AND p2.amopstrategy = 3 AND
    p1.amoplefttype != p1.amoprighttype AND
    p2.amoplefttype != p2.amoprighttype AND
    NOT EXISTS(SELECT 1 FROM pg_amop p3 WHERE
                 p3.amopfamily = p1.amopfamily AND
                 p3.amoplefttype = p1.amoplefttype AND
                 p3.amoprighttype = p2.amoprighttype AND
                 p3.amopstrategy = 3);

-- **************** pg_amproc ****************

-- Look for illegal values in pg_amproc fields

SELECT p1.amprocfamily, p1.amprocnum
FROM pg_amproc as p1
WHERE p1.amprocfamily = 0 OR p1.amproclefttype = 0 OR p1.amprocrighttype = 0
    OR p1.amprocnum < 1 OR p1.amproc = 0;

-- Cross-check amprocnum index against parent AM

SELECT p1.amprocfamily, p1.amprocnum, p2.oid, p2.amname
FROM pg_amproc AS p1, pg_am AS p2, pg_opfamily AS p3
WHERE p1.amprocfamily = p3.oid AND p3.opfmethod = p2.oid AND
    p1.amprocnum > p2.amsupport;

-- Detect missing pg_amproc entries: should have as many support functions
-- as AM expects for each datatype combination supported by the opfamily.

SELECT p1.amname, p2.opfname, p3.amproclefttype, p3.amprocrighttype
FROM pg_am AS p1, pg_opfamily AS p2, pg_amproc AS p3
WHERE p2.opfmethod = p1.oid AND p3.amprocfamily = p2.oid AND
    p1.amsupport != (SELECT count(*) FROM pg_amproc AS p4
                     WHERE p4.amprocfamily = p2.oid AND
                           p4.amproclefttype = p3.amproclefttype AND
                           p4.amprocrighttype = p3.amprocrighttype);

-- Unfortunately, we can't check the amproc link very well because the
-- signature of the function may be different for different support routines
-- or different base data types.
-- We can check that all the referenced instances of the same support
-- routine number take the same number of parameters, but that's about it
-- for a general check...

SELECT p1.amprocfamily, p1.amprocnum,
	p2.oid, p2.proname,
	p3.opfname,
	p4.amprocfamily, p4.amprocnum,
	p5.oid, p5.proname,
	p6.opfname
FROM pg_amproc AS p1, pg_proc AS p2, pg_opfamily AS p3,
     pg_amproc AS p4, pg_proc AS p5, pg_opfamily AS p6
WHERE p1.amprocfamily = p3.oid AND p4.amprocfamily = p6.oid AND
    p3.opfmethod = p6.opfmethod AND p1.amprocnum = p4.amprocnum AND
    p1.amproc = p2.oid AND p4.amproc = p5.oid AND
    (p2.proretset OR p5.proretset OR p2.pronargs != p5.pronargs);

-- For btree, though, we can do better since we know the support routines
-- must be of the form cmp(lefttype, righttype) returns int4.

SELECT p1.amprocfamily, p1.amprocnum,
	p2.oid, p2.proname,
	p3.opfname
FROM pg_amproc AS p1, pg_proc AS p2, pg_opfamily AS p3
WHERE p3.opfmethod = (SELECT oid FROM pg_am WHERE amname = 'btree')
    AND p1.amprocfamily = p3.oid AND p1.amproc = p2.oid AND
    (amprocnum != 1
     OR proretset
     OR prorettype != 'int4'::regtype
     OR pronargs != 2
     OR proargtypes[0] != amproclefttype
     OR proargtypes[1] != amprocrighttype);

-- For hash we can also do a little better: the support routines must be
-- of the form hash(lefttype) returns int4.  There are several cases where
-- we cheat and use a hash function that is physically compatible with the
-- datatype even though there's no cast, so for now we can't check that.

SELECT p1.amprocfamily, p1.amprocnum,
	p2.oid, p2.proname,
	p3.opfname
FROM pg_amproc AS p1, pg_proc AS p2, pg_opfamily AS p3
WHERE p3.opfmethod = (SELECT oid FROM pg_am WHERE amname = 'hash')
    AND p1.amprocfamily = p3.oid AND p1.amproc = p2.oid AND
    (amprocnum != 1
     OR proretset
     OR prorettype != 'int4'::regtype
     OR pronargs != 1
--   OR NOT physically_coercible(amproclefttype, proargtypes[0])
     OR amproclefttype != amprocrighttype);

-- Support routines that are primary members of opfamilies must be immutable
-- (else it suggests that the index ordering isn't fixed).  But cross-type
-- members need only be stable, since they are just shorthands
-- for index probe queries.

SELECT p1.amprocfamily, p1.amproc, p2.prosrc
FROM pg_amproc AS p1, pg_proc AS p2
WHERE p1.amproc = p2.oid AND
    p1.amproclefttype = p1.amprocrighttype AND
    p2.provolatile != 'i';

SELECT p1.amprocfamily, p1.amproc, p2.prosrc
FROM pg_amproc AS p1, pg_proc AS p2
WHERE p1.amproc = p2.oid AND
    p1.amproclefttype != p1.amprocrighttype AND
    p2.provolatile = 'v';

-- Check for oid collisions between pg_proc/pg_type/pg_class
SELECT *
FROM
  (SELECT *, count(*) over (partition by oid) as oid_count
   FROM (select oid, 'pg_proc' as catalog, proname as name from pg_proc
         union all
         select oid, 'pg_type' as catalog, typname as name from pg_type
         union all
         select oid, 'pg_class' as catalgo, relname as name from pg_class) cats
  ) cat_counts
WHERE oid_count > 1;
