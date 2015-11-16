-- OIDS 1 - 99 
 CREATE TYPE bool(
   INPUT = boolin,
   OUTPUT = boolout,
   RECEIVE = boolrecv,
   SEND = boolsend,
   INTERNALLENGTH = 1,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = char
 ) WITH (OID=16, ARRAYOID=1000, DESCRIPTION="boolean, 'true'/'false'");
-- #define BOOLOID			16

 CREATE TYPE bytea(
   INPUT = byteain,
   OUTPUT = byteaout,
   RECEIVE = bytearecv,
   SEND = byteasend,
   INTERNALLENGTH = VARIABLE,
   STORAGE = extended,
   ALIGNMENT = int4
 ) WITH (OID=17, ARRAYOID=1001, DESCRIPTION="variable-length string, binary values escaped");
-- #define BYTEAOID		17

 CREATE TYPE "char"(
   INPUT = charin,
   OUTPUT = charout,
   RECEIVE = charrecv,
   SEND = charsend,
   INTERNALLENGTH = 1,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = char
 ) WITH (OID=18, ARRAYOID=1002, DESCRIPTION="single character");
-- #define CHAROID			18

 CREATE TYPE name(
   INPUT = namein,
   OUTPUT = nameout,
   RECEIVE = namerecv,
   SEND = namesend,
   INTERNALLENGTH = 64,
   STORAGE = plain,
   ELEMENT = char,
   ALIGNMENT = int4
 ) WITH (OID=19, ARRAYOID=1003, DESCRIPTION="63-character type for storing system identifiers");
-- #define NAMEOID			19

 CREATE TYPE int8(
   INPUT = int8in,
   OUTPUT = int8out,
   RECEIVE = int8recv,
   SEND = int8send,
   INTERNALLENGTH = 8,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = double
 ) WITH (OID=20, ARRAYOID=1016, DESCRIPTION="~18 digit integer, 8-byte storage");
-- #define INT8OID			20

 CREATE TYPE int2(
   INPUT = int2in,
   OUTPUT = int2out,
   RECEIVE = int2recv,
   SEND = int2send,
   INTERNALLENGTH = 2,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int2
 ) WITH (OID=21, ARRAYOID=1005, DESCRIPTION="-32 thousand to 32 thousand, 2-byte storage");
-- #define INT2OID			21

 CREATE TYPE int2vector(
   INPUT = int2vectorin,
   OUTPUT = int2vectorout,
   RECEIVE = int2vectorrecv,
   SEND = int2vectorsend,
   INTERNALLENGTH = VARIABLE,
   STORAGE = plain,
   ELEMENT = int2,
   ALIGNMENT = int4
 ) WITH (OID=22, ARRAYOID=1006, DESCRIPTION="array of int2, used in system tables");
-- #define INT2VECTOROID	22

 CREATE TYPE int4(
   INPUT = int4in,
   OUTPUT = int4out,
   RECEIVE = int4recv,
   SEND = int4send,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=23, ARRAYOID=1007, DESCRIPTION="-2 billion to 2 billion integer, 4-byte storage");
-- #define INT4OID			23

 CREATE TYPE regproc(
   INPUT = regprocin,
   OUTPUT = regprocout,
   RECEIVE = regprocrecv,
   SEND = regprocsend,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=24, ARRAYOID=1008, DESCRIPTION="registered procedure");
-- #define REGPROCOID		24

 CREATE TYPE text(
   INPUT = textin,
   OUTPUT = textout,
   RECEIVE = textrecv,
   SEND = textsend,
   INTERNALLENGTH = VARIABLE,
   STORAGE = extended,
   ALIGNMENT = int4
 ) WITH (OID=25, ARRAYOID=1009, DESCRIPTION="variable-length string, no limit specified");
-- #define TEXTOID			25

 CREATE TYPE oid(
   INPUT = oidin,
   OUTPUT = oidout,
   RECEIVE = oidrecv,
   SEND = oidsend,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=26, ARRAYOID=1028, DESCRIPTION="object identifier(oid), maximum 4 billion");
-- #define OIDOID			26

 CREATE TYPE tid(
   INPUT = tidin,
   OUTPUT = tidout,
   RECEIVE = tidrecv,
   SEND = tidsend,
   INTERNALLENGTH = 6,
   STORAGE = plain,
   ALIGNMENT = int2
 ) WITH (OID=27, ARRAYOID=1010, DESCRIPTION="(Block, offset), physical location of tuple");
-- #define TIDOID		27

 CREATE TYPE xid(
   INPUT = xidin,
   OUTPUT = xidout,
   RECEIVE = xidrecv,
   SEND = xidsend,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=28, ARRAYOID=1011, DESCRIPTION="transaction id");
-- #define XIDOID 28

 CREATE TYPE cid(
   INPUT = cidin,
   OUTPUT = cidout,
   RECEIVE = cidrecv,
   SEND = cidsend,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=29, ARRAYOID=1012, DESCRIPTION="command identifier type, sequence in transaction id");
-- #define CIDOID 29

 CREATE TYPE oidvector(
   INPUT = oidvectorin,
   OUTPUT = oidvectorout,
   RECEIVE = oidvectorrecv,
   SEND = oidvectorsend,
   INTERNALLENGTH = VARIABLE,
   STORAGE = plain,
   ELEMENT = oid,
   ALIGNMENT = int4
 ) WITH (OID=30, ARRAYOID=1013, DESCRIPTION="array of oids, used in system tables");
-- #define OIDVECTOROID	30

-- hand-built rowtype entries for bootstrapped catalogs: 
 CREATE TYPE pg_type(
 ) WITH (OID=71, RELID=1247);
-- #define PG_TYPE_RELTYPE_OID 71

 CREATE TYPE pg_attribute(
 ) WITH (OID=75, RELID=1249);
-- #define PG_ATTRIBUTE_RELTYPE_OID 75

 CREATE TYPE pg_proc(
 ) WITH (OID=81, RELID=1255);
-- #define PG_PROC_RELTYPE_OID 81

 CREATE TYPE pg_class(
 ) WITH (OID=83, RELID=1259);
-- #define PG_CLASS_RELTYPE_OID 83

 CREATE TYPE pg_authid(
 ) WITH (OID=2922, RELID=1260);
-- #define PG_AUTHID_RELTYPE_OID 2922

 CREATE TYPE pg_auth_members(
 ) WITH (OID=2923, RELID=1261);
-- #define PG_AUTH_MEMBERS_RELTYPE_OID 2923

 CREATE TYPE pg_database(
 ) WITH (OID=2924, RELID=1262);
-- #define PG_DATABASE_RELTYPE_OID 2924

 CREATE TYPE gp_global_sequence(
 ) WITH (OID=6995, RELID=5096);
-- #define GP_GLOBAL_SEQUENCE_RELTYPE_OID 6995

-- OIDS 100 - 199 
 CREATE TYPE xml(
   INPUT = xml_in,
   OUTPUT = xml_out,
   RECEIVE = xml_recv,
   SEND = xml_send,
   INTERNALLENGTH = VARIABLE,
   STORAGE = extended,
   ALIGNMENT = int4
 ) WITH (OID=142, ARRAYOID=143, DESCRIPTION="XML content");
-- #define XMLOID 142

-- OIDS 200 - 299 

 CREATE TYPE smgr(
   INPUT = smgrin,
   OUTPUT = smgrout,
   INTERNALLENGTH = 2,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int2
 ) WITH (OID=210, DESCRIPTION="storage manager");

 DROP TYPE _smgr;


-- OIDS 300 - 399 

-- OIDS 400 - 499 

-- OIDS 500 - 599 

-- OIDS 600 - 699 

 CREATE TYPE point(
   INPUT = point_in,
   OUTPUT = point_out,
   RECEIVE = point_recv,
   SEND = point_send,
   INTERNALLENGTH = 16,
   STORAGE = plain,
   ELEMENT = float8,
   ALIGNMENT = double
 ) WITH (OID=600, ARRAYOID=1017, DESCRIPTION="geometric point '(x, y)'");
-- #define POINTOID		600

 CREATE TYPE lseg(
   INPUT = lseg_in,
   OUTPUT = lseg_out,
   RECEIVE = lseg_recv,
   SEND = lseg_send,
   INTERNALLENGTH = 32,
   STORAGE = plain,
   ELEMENT = point,
   ALIGNMENT = double
 ) WITH (OID=601, ARRAYOID=1018, DESCRIPTION="geometric line segment '(pt1,pt2)'");
-- #define LSEGOID			601

 CREATE TYPE path(
   INPUT = path_in,
   OUTPUT = path_out,
   RECEIVE = path_recv,
   SEND = path_send,
   INTERNALLENGTH = VARIABLE,
   STORAGE = extended,
   ALIGNMENT = double
 ) WITH (OID=602, ARRAYOID=1019, DESCRIPTION="geometric path '(pt1,...)'");
-- #define PATHOID			602

 CREATE TYPE box(
   INPUT = box_in,
   OUTPUT = box_out,
   RECEIVE = box_recv,
   SEND = box_send,
   INTERNALLENGTH = 32,
   STORAGE = plain,
   ELEMENT = point,
   DELIMITER = ';',
   ALIGNMENT = double
 ) WITH (OID=603, ARRAYOID=1020, DESCRIPTION="geometric box '(lower left,upper right)'");
-- #define BOXOID			603

 CREATE TYPE polygon(
   INPUT = poly_in,
   OUTPUT = poly_out,
   RECEIVE = poly_recv,
   SEND = poly_send,
   INTERNALLENGTH = VARIABLE,
   STORAGE = extended,
   ALIGNMENT = double
 ) WITH (OID=604, ARRAYOID=1027, DESCRIPTION="geometric polygon '(pt1,...)'");
-- #define POLYGONOID		604

 CREATE TYPE line(
   INPUT = line_in,
   OUTPUT = line_out,
   RECEIVE = line_recv,
   SEND = line_send,
   INTERNALLENGTH = 32,
   STORAGE = plain,
   ELEMENT = float8,
   ALIGNMENT = double
 ) WITH (OID=628, ARRAYOID=629, DESCRIPTION="geometric line (not implemented)");
-- #define LINEOID			628

-- OIDS 700 - 799 

 CREATE TYPE float4(
   INPUT = float4in,
   OUTPUT = float4out,
   RECEIVE = float4recv,
   SEND = float4send,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=700, ARRAYOID=1021, DESCRIPTION="single-precision floating point number, 4-byte storage");
-- #define FLOAT4OID 700

 CREATE TYPE float8(
   INPUT = float8in,
   OUTPUT = float8out,
   RECEIVE = float8recv,
   SEND = float8send,
   INTERNALLENGTH = 8,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = double
 ) WITH (OID=701, ARRAYOID=1022, DESCRIPTION="double-precision floating point number, 8-byte storage");
-- #define FLOAT8OID 701

 CREATE TYPE abstime(
   INPUT = abstimein,
   OUTPUT = abstimeout,
   RECEIVE = abstimerecv,
   SEND = abstimesend,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=702, ARRAYOID=1023, DESCRIPTION="absolute, limited-range date and time (Unix system time)");
-- #define ABSTIMEOID		702

 CREATE TYPE reltime(
   INPUT = reltimein,
   OUTPUT = reltimeout,
   RECEIVE = reltimerecv,
   SEND = reltimesend,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=703, ARRAYOID=1024, DESCRIPTION="relative, limited-range time interval (Unix delta time)");
-- #define RELTIMEOID		703

 CREATE TYPE tinterval(
   INPUT = tintervalin,
   OUTPUT = tintervalout,
   RECEIVE = tintervalrecv,
   SEND = tintervalsend,
   INTERNALLENGTH = 12,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=704, ARRAYOID=1025, DESCRIPTION="(abstime,abstime), time interval");
-- #define TINTERVALOID	704

 CREATE TYPE unknown(
   INPUT = unknownin,
   OUTPUT = unknownout,
   RECEIVE = unknownrecv,
   SEND = unknownsend,
   INTERNALLENGTH = -2,
   STORAGE = plain,
   ALIGNMENT = char
 ) WITH (OID=705, DESCRIPTION="");
-- #define UNKNOWNOID		705

 DROP TYPE _unknown;

 CREATE TYPE circle(
   INPUT = circle_in,
   OUTPUT = circle_out,
   RECEIVE = circle_recv,
   SEND = circle_send,
   INTERNALLENGTH = 24,
   STORAGE = plain,
   ALIGNMENT = double
 ) WITH (OID=718, ARRAYOID=719, DESCRIPTION="geometric circle '(center,radius)'");
-- #define CIRCLEOID		718

 CREATE TYPE money(
   INPUT = cash_in,
   OUTPUT = cash_out,
   RECEIVE = cash_recv,
   SEND = cash_send,
   INTERNALLENGTH = 8,
   STORAGE = plain,
   ALIGNMENT = double
 ) WITH (OID=790, ARRAYOID=791, DESCRIPTION="monetary amounts, $d,ddd.cc");
-- #define CASHOID 790


-- OIDS 800 - 899 
 CREATE TYPE macaddr(
   INPUT = macaddr_in,
   OUTPUT = macaddr_out,
   RECEIVE = macaddr_recv,
   SEND = macaddr_send,
   INTERNALLENGTH = 6,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=829, ARRAYOID=1040, DESCRIPTION="XX:XX:XX:XX:XX:XX, MAC address");
-- #define MACADDROID 829

 CREATE TYPE inet(
   INPUT = inet_in,
   OUTPUT = inet_out,
   RECEIVE = inet_recv,
   SEND = inet_send,
   INTERNALLENGTH = VARIABLE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=869, ARRAYOID=1041, DESCRIPTION="IP address/netmask, host address, netmask optional");
-- #define INETOID 869

 CREATE TYPE cidr(
   INPUT = cidr_in,
   OUTPUT = cidr_out,
   RECEIVE = cidr_recv,
   SEND = cidr_send,
   INTERNALLENGTH = VARIABLE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=650, ARRAYOID=651, DESCRIPTION="network IP address/netmask, network address");
-- #define CIDROID 650

-- OIDS 900 - 999 

-- OIDS 1000 - 1099 

-- ARRAY TYPE bool
-- ARRAY TYPE bytea
-- ARRAY TYPE char
-- ARRAY TYPE name
-- ARRAY TYPE int2
-- #define INT2ARRAYOID 1005
-- ARRAY TYPE int2vector
-- ARRAY TYPE int4
-- #define INT4ARRAYOID		1007
-- ARRAY TYPE regproc
-- ARRAY TYPE text
-- #define TEXTARRAYOID		1009
-- ARRAY TYPE oid
-- #define OIDARRAYOID		1028
-- ARRAY TYPE tid
-- ARRAY TYPE xid
-- ARRAY TYPE cid
-- ARRAY TYPE cstring
-- ARRAY TYPE oidvector
-- ARRAY TYPE bpchar
-- ARRAY TYPE varchar
-- ARRAY TYPE int8
-- #define INT8ARRAYOID		1016
-- ARRAY TYPE point
-- ARRAY TYPE lseg
-- ARRAY TYPE path
-- ARRAY TYPE box
-- ARRAY TYPE float4
-- #define FLOAT4ARRAYOID 1021
-- ARRAY TYPE float8
-- #define FLOAT8ARRAYOID 1022
-- ARRAY TYPE abstime
-- ARRAY TYPE reltime
-- ARRAY TYPE tinterval
-- ARRAY TYPE polygon

 CREATE TYPE aclitem(
   INPUT = aclitemin,
   OUTPUT = aclitemout,
   INTERNALLENGTH = 12,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=1033, ARRAYOID=1034, DESCRIPTION="access control list");
-- #define ACLITEMOID		1033
-- ARRAY TYPE macaddr
-- ARRAY TYPE inet
-- ARRAY TYPE cidr

 CREATE TYPE bpchar(
   INPUT = bpcharin,
   OUTPUT = bpcharout,
   RECEIVE = bpcharrecv,
   SEND = bpcharsend,
   TYPMODIN = bpchartypmodin,
   TYPMODOUT = bpchartypmodout,
   INTERNALLENGTH = VARIABLE,
   STORAGE = extended,
   ALIGNMENT = int4
 ) WITH (OID=1042, ARRAYOID=1014, DESCRIPTION="char(length), blank-padded string, fixed storage length");
-- #define BPCHAROID		1042

 CREATE TYPE "varchar"(
   INPUT = varcharin,
   OUTPUT = varcharout,
   RECEIVE = varcharrecv,
   SEND = varcharsend,
   TYPMODIN = varchartypmodin,
   TYPMODOUT = varchartypmodout,
   INTERNALLENGTH = VARIABLE,
   STORAGE = extended,
   ALIGNMENT = int4
 ) WITH (OID=1043, ARRAYOID=1015, DESCRIPTION="varchar(length), non-blank-padded string, variable storage length");
-- #define VARCHAROID		1043

 CREATE TYPE date(
   INPUT = date_in,
   OUTPUT = date_out,
   RECEIVE = date_recv,
   SEND = date_send,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=1082, ARRAYOID=1182, DESCRIPTION="ANSI SQL date");
-- #define DATEOID			1082

 CREATE TYPE "time"(
   INPUT = time_in,
   OUTPUT = time_out,
   RECEIVE = time_recv,
   SEND = time_send,
   TYPMODIN = timetypmodin,
   TYPMODOUT = timetypmodout,
   INTERNALLENGTH = 8,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = double
 ) WITH (OID=1083, ARRAYOID=1183, DESCRIPTION="hh:mm:ss, ANSI SQL time");
-- #define TIMEOID			1083

-- OIDS 1100 - 1199 
 CREATE TYPE "timestamp"(
   INPUT = timestamp_in,
   OUTPUT = timestamp_out,
   RECEIVE = timestamp_recv,
   SEND = timestamp_send,
   TYPMODIN = timestamptypmodin,
   TYPMODOUT = timestamptypmodout,
   INTERNALLENGTH = 8,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = double
 ) WITH (OID=1114, ARRAYOID=1115, DESCRIPTION="date and time");
-- #define TIMESTAMPOID	1114

-- ARRAY TYPE date
-- ARRAY TYPE time

 CREATE TYPE timestamptz(
   INPUT = timestamptz_in,
   OUTPUT = timestamptz_out,
   RECEIVE = timestamptz_recv,
   SEND = timestamptz_send,
   TYPMODIN = timestamptztypmodin,
   TYPMODOUT = timestamptztypmodout,
   INTERNALLENGTH = 8,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = double
 ) WITH (OID=1184, ARRAYOID=1185, DESCRIPTION="date and time with time zone");
-- #define TIMESTAMPTZOID	1184

 CREATE TYPE "interval"(
   INPUT = interval_in,
   OUTPUT = interval_out,
   RECEIVE = interval_recv,
   SEND = interval_send,
   TYPMODIN = intervaltypmodin,
   TYPMODOUT = intervaltypmodout,
   INTERNALLENGTH = 16,
   STORAGE = plain,
   ALIGNMENT = double
 ) WITH (OID=1186, ARRAYOID=1187, DESCRIPTION="@ <number> <units>, time interval");
-- #define INTERVALOID		1186

-- OIDS 1200 - 1299 
-- ARRAY TYPE numeric

 CREATE TYPE timetz(
   INPUT = timetz_in,
   OUTPUT = timetz_out,
   RECEIVE = timetz_recv,
   SEND = timetz_send,
   TYPMODIN = timetztypmodin,
   TYPMODOUT = timetztypmodout,
   INTERNALLENGTH = 12,
   STORAGE = plain,
   ALIGNMENT = double
 ) WITH (OID=1266, ARRAYOID=1270, DESCRIPTION="hh:mm:ss, ANSI SQL time");
-- #define TIMETZOID		1266

-- OIDS 1500 - 1599 
 CREATE TYPE "bit"(
   INPUT = bit_in,
   OUTPUT = bit_out,
   RECEIVE = bit_recv,
   SEND = bit_send,
   TYPMODIN = bittypmodin,
   TYPMODOUT = bittypmodout,
   INTERNALLENGTH = VARIABLE,
   STORAGE = extended,
   ALIGNMENT = int4
 ) WITH (OID=1560, ARRAYOID=1561, DESCRIPTION="fixed-length bit string");
-- #define BITOID	 1560

 CREATE TYPE varbit(
   INPUT = varbit_in,
   OUTPUT = varbit_out,
   RECEIVE = varbit_recv,
   SEND = varbit_send,
   TYPMODIN = varbittypmodin,
   TYPMODOUT = varbittypmodout,
   INTERNALLENGTH = VARIABLE,
   STORAGE = extended,
   ALIGNMENT = int4
 ) WITH (OID=1562, ARRAYOID=1563, DESCRIPTION="variable-length bit string");
-- #define VARBITOID	  1562

-- OIDS 1600 - 1699 

-- OIDS 1700 - 1799 
 CREATE TYPE "numeric"(
   INPUT = numeric_in,
   OUTPUT = numeric_out,
   RECEIVE = numeric_recv,
   SEND = numeric_send,
   TYPMODIN = numerictypmodin,
   TYPMODOUT = numerictypmodout,
   INTERNALLENGTH = VARIABLE,
   STORAGE = main,
   ALIGNMENT = int4
 ) WITH (OID=1700, ARRAYOID=1231, DESCRIPTION="numeric(precision, decimal), arbitrary precision number");
-- #define NUMERICOID		1700

 CREATE TYPE refcursor(
   INPUT = dummy_cast_functions.textin,
   OUTPUT = dummy_cast_functions.textout,
   RECEIVE = dummy_cast_functions.textrecv,
   SEND = dummy_cast_functions.textsend,
   INTERNALLENGTH = VARIABLE,
   STORAGE = extended,
   ALIGNMENT = int4
 ) WITH (OID=1790, ARRAYOID=2201, DESCRIPTION="reference cursor (portal name)");
-- #define REFCURSOROID	1790

-- OIDS 2200 - 2299 
-- ARRAY TYPE refcursor

 CREATE TYPE regprocedure(
   INPUT = regprocedurein,
   OUTPUT = regprocedureout,
   RECEIVE = regprocedurerecv,
   SEND = regproceduresend,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=2202, ARRAYOID=2207, DESCRIPTION="registered procedure (with args)");
-- #define REGPROCEDUREOID 2202

 CREATE TYPE regoper(
   INPUT = regoperin,
   OUTPUT = regoperout,
   RECEIVE = regoperrecv,
   SEND = regopersend,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=2203, ARRAYOID=2208, DESCRIPTION="registered operator");
-- #define REGOPEROID		2203

 CREATE TYPE regoperator(
   INPUT = regoperatorin,
   OUTPUT = regoperatorout,
   RECEIVE = regoperatorrecv,
   SEND = regoperatorsend,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=2204, ARRAYOID=2209, DESCRIPTION="registered operator (with args)");
-- #define REGOPERATOROID	2204

 CREATE TYPE regclass(
   INPUT = regclassin,
   OUTPUT = regclassout,
   RECEIVE = regclassrecv,
   SEND = regclasssend,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=2205, ARRAYOID=2210, DESCRIPTION="registered class");
-- #define REGCLASSOID		2205

 CREATE TYPE regtype(
   INPUT = regtypein,
   OUTPUT = regtypeout,
   RECEIVE = regtyperecv,
   SEND = regtypesend,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=2206, ARRAYOID=2211, DESCRIPTION="registered type");
-- #define REGTYPEOID		2206

-- ARRAY TYPE regprocedure
-- ARRAY TYPE regoper
-- ARRAY TYPE regoperator
-- ARRAY TYPE regclass
-- ARRAY TYPE regtype
-- #define REGTYPEARRAYOID 2211

 CREATE TYPE nb_classification(
 ) WITH (OID=3251, RELID=3250);

 CREATE TYPE gpaotid(
   INPUT = gpaotidin,
   OUTPUT = gpaotidout,
   RECEIVE = gpaotidrecv,
   SEND = gpaotidsend,
   INTERNALLENGTH = 6,
   STORAGE = plain,
   ALIGNMENT = int2
 ) WITH (OID=3300, ARRAYOID=3301, DESCRIPTION="(segment file num, row number), logical location of append-only tuple");
-- #define AOTIDOID	3300

 CREATE TYPE gpxlogloc(
   INPUT = gpxloglocin,
   OUTPUT = gpxloglocout,
   RECEIVE = gpxloglocrecv,
   SEND = gpxloglocsend,
   INTERNALLENGTH = 8,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=3310, ARRAYOID=3311, DESCRIPTION="(h/h) -- the hexadecimal xlogid and xrecoff of an XLOG location");
-- #define XLOGLOCOID	3310

--
-- pseudo-types
--
-- types with typtype='p' represent various special cases in the type system.
--
-- These cannot be used to define table columns, but are valid as function
-- argument and result types (if supported by the function's implementation
-- language).
--
-- Note: cstring is a borderline case; it is still considered a pseudo-type,
-- but there is now support for it in records and arrays.  Perhaps we should
-- just treat it as a regular base type?


 CREATE TYPE record(
   INPUT = record_in,
   OUTPUT = record_out,
   RECEIVE = record_recv,
   SEND = record_send,
   INTERNALLENGTH = VARIABLE,
   STORAGE = extended,
   ALIGNMENT = double
 ) WITH (OID=2249, TYPTYPE=PSEUDO);
-- #define RECORDOID		2249

 CREATE TYPE cstring(
   INPUT = cstring_in,
   OUTPUT = cstring_out,
   RECEIVE = cstring_recv,
   SEND = cstring_send,
   INTERNALLENGTH = -2,
   STORAGE = plain,
   ALIGNMENT = char
 ) WITH (OID=2275, ARRAYOID=1263, TYPTYPE=PSEUDO);
-- #define CSTRINGOID		2275
-- #define CSTRINGARRAYOID	1263

 CREATE TYPE "any"(
   INPUT = any_in,
   OUTPUT = any_out,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=2276, TYPTYPE=PSEUDO);
-- #define ANYOID			2276

 CREATE TYPE anyarray(
   INPUT = anyarray_in,
   OUTPUT = anyarray_out,
   RECEIVE = anyarray_recv,
   SEND = anyarray_send,
   INTERNALLENGTH = VARIABLE,
   STORAGE = extended,
   ALIGNMENT = double
 ) WITH (OID=2277, TYPTYPE=PSEUDO);
-- #define ANYARRAYOID		2277

 CREATE TYPE void(
   INPUT = void_in,
   OUTPUT = void_out,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=2278, TYPTYPE=PSEUDO);
-- #define VOIDOID			2278

 CREATE TYPE trigger(
   INPUT = trigger_in,
   OUTPUT = trigger_out,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=2279, TYPTYPE=PSEUDO);
-- #define TRIGGEROID		2279

 CREATE TYPE language_handler(
   INPUT = language_handler_in,
   OUTPUT = language_handler_out,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=2280, TYPTYPE=PSEUDO);
-- #define LANGUAGE_HANDLEROID		2280

 CREATE TYPE internal(
   INPUT = internal_in,
   OUTPUT = internal_out,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=2281, TYPTYPE=PSEUDO);
-- #define INTERNALOID		2281

 CREATE TYPE opaque(
   INPUT = opaque_in,
   OUTPUT = opaque_out,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=2282, TYPTYPE=PSEUDO);
-- #define OPAQUEOID		2282

 CREATE TYPE anyelement(
   INPUT = anyelement_in,
   OUTPUT = anyelement_out,
   INTERNALLENGTH = 4,
   PASSEDBYVALUE,
   STORAGE = plain,
   ALIGNMENT = int4
 ) WITH (OID=2283, TYPTYPE=PSEUDO);
-- #define ANYELEMENTOID	2283

 CREATE TYPE anytable(
   INPUT = anytable_in,
   OUTPUT = anytable_out,
   INTERNALLENGTH = VARIABLE,
   STORAGE = extended,
   ALIGNMENT = double
 ) WITH (OID=3053, DESCRIPTION="Represents a generic TABLE value expression", TYPTYPE=PSEUDO);
-- #define ANYTABLEOID     3053

