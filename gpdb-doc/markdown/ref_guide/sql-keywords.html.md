# Reserved Identifiers and SQL Key Words 

This topic describes Greenplum Database reserved identifiers and object names, and SQL key words recognized by the Greenplum Database and PostgreSQL command parsers.

## <a id="resident"></a>Reserved Identifiers 

In the Greenplum Database system, names beginning with `gp_` and `pg_` are reserved and should not be used as names for user-created objects, such as tables, views, and functions.

The resource group names `admin_group`, `default_group`, and `none` are reserved. The resource queue name `pg_default` is reserved.

The tablespace names `pg_default` and `pg_global` are reserved.

The role name `gpadmin` is reserved. `gpadmin` is the default Greenplum Database superuser role.

In data files, the characters that delimit fields \(columns\) and rows have a special meaning. If they appear within the data you must escape them so that Greenplum Database treats them as data and not as delimiters. The backslash character \(`\`\) is the default escape character. See [Escaping](../admin_guide/load/topics/g-escaping.html#topic99) for details.

See [SQL Syntax](https://www.postgresql.org/docs/12/sql-syntax.html) in the PostgreSQL documentation for more information about SQL identifiers, constants, operators, and expressions.

## <a id="keywords"></a>SQL Key Words 

[Table 1](#table_yjz_3hb_jhb) lists all tokens that are key words in Greenplum Database 6 and PostgreSQL 9.4.

ANSI SQL distinguishes between *reserved* and *unreserved* key words. According to the standard, reserved key words are the only real key words; they are never allowed as identifiers. Unreserved key words only have a special meaning in particular contexts and can be used as identifiers in other contexts. Most unreserved key words are actually the names of built-in tables and functions specified by SQL. The concept of unreserved key words essentially only exists to declare that some predefined meaning is attached to a word in some contexts.

In the Greenplum Database and PostgreSQL parsers there are several different classes of tokens ranging from those that can never be used as an identifier to those that have absolutely no special status in the parser as compared to an ordinary identifier. \(The latter is usually the case for functions specified by SQL.\) Even reserved key words are not completely reserved, but can be used as column labels \(for example, `SELECT 55 AS CHECK`, even though `CHECK` is a reserved key word\).

[Table 1](#table_yjz_3hb_jhb) classifies as "unreserved" those key words that are explicitly known to the parser but are allowed as column or table names. Some key words that are otherwise unreserved cannot be used as function or data type names and are marked accordingly. \(Most of these words represent built-in functions or data types with special syntax. The function or type is still available but it cannot be redefined by the user.\) Key words labeled "reserved" are not allowed as column or table names. Some reserved key words are allowable as names for functions or data types; this is also shown in the table. If not so marked, a reserved key word is only allowed as an "AS" column label name.

If you get spurious parser errors for commands that contain any of the listed key words as an identifier you should try to quote the identifier to see if the problem goes away.

Before studying the table, note the fact that a key word is not reserved does not mean that the feature related to the word is not implemented. Conversely, the presence of a key word does not indicate the existence of a feature.

|Key Word|Greenplum Database|PostgreSQL 9.4|
|:-------|:-----------------|:-------------|
|ABORT|unreserved|unreserved|
|ABSOLUTE|unreserved|unreserved|
|ACCESS|unreserved|unreserved|
|ACTION|unreserved|unreserved|
|ACTIVE|unreserved||
|ADD|unreserved|unreserved|
|ADMIN|unreserved|unreserved|
|AFTER|unreserved|unreserved|
|AGGREGATE|unreserved|unreserved|
|ALL|reserved|reserved|
|ALSO|unreserved|unreserved|
|ALTER|unreserved|unreserved|
|ALWAYS|unreserved|unreserved|
|ANALYSE|reserved|reserved|
|ANALYZE|reserved|reserved|
|AND|reserved|reserved|
|ANY|reserved|reserved|
|ARRAY|reserved|reserved|
|AS|reserved|reserved|
|ASC|reserved|reserved|
|ASSERTION|unreserved|unreserved|
|ASSIGNMENT|unreserved|unreserved|
|ASYMMETRIC|reserved|reserved|
|AT|unreserved|unreserved|
|ATTRIBUTE|unreserved|unreserved|
|AUTHORIZATION|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|BACKWARD|unreserved|unreserved|
|BEFORE|unreserved|unreserved|
|BEGIN|unreserved|unreserved|
|BETWEEN|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|BIGINT|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|BINARY|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|BIT|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|BOOLEAN|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|BOTH|reserved|reserved|
|BY|unreserved|unreserved|
|CACHE|unreserved|unreserved|
|CALLED|unreserved|unreserved|
|CASCADE|unreserved|unreserved|
|CASCADED|unreserved|unreserved|
|CASE|reserved|reserved|
|CAST|reserved|reserved|
|CATALOG|unreserved|unreserved|
|CHAIN|unreserved|unreserved|
|CHAR|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|CHARACTER|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|CHARACTERISTICS|unreserved|unreserved|
|CHECK|reserved|reserved|
|CHECKPOINT|unreserved|unreserved|
|CLASS|unreserved|unreserved|
|CLOSE|unreserved|unreserved|
|CLUSTER|unreserved|unreserved|
|COALESCE|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|COLLATE|reserved|reserved|
|COLLATION|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|COLUMN|reserved|reserved|
|COMMENT|unreserved|unreserved|
|COMMENTS|unreserved|unreserved|
|COMMIT|unreserved|unreserved|
|COMMITTED|unreserved|unreserved|
|CONCURRENCY|unreserved||
|CONCURRENTLY|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|CONFIGURATION|unreserved|unreserved|
|CONFLICT|unreserved|unreserved|
|CONNECTION|unreserved|unreserved|
|CONSTRAINT|reserved|reserved|
|CONSTRAINTS|unreserved|unreserved|
|CONTAINS|unreserved||
|CONTENT|unreserved|unreserved|
|CONTINUE|unreserved|unreserved|
|CONVERSION|unreserved|unreserved|
|COPY|unreserved|unreserved|
|COST|unreserved|unreserved|
|CPU\_RATE\_LIMIT|unreserved||
|CPUSET|unreserved||
|CREATE|reserved|reserved|
|CREATEEXTTABLE|unreserved||
|CROSS|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|CSV|unreserved|unreserved|
|CUBE|unreserved \(cannot be function or type name\)||
|CURRENT|unreserved|unreserved|
|CURRENT\_CATALOG|reserved|reserved|
|CURRENT\_DATE|reserved|reserved|
|CURRENT\_ROLE|reserved|reserved|
|CURRENT\_SCHEMA|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|CURRENT\_TIME|reserved|reserved|
|CURRENT\_TIMESTAMP|reserved|reserved|
|CURRENT\_USER|reserved|reserved|
|CURSOR|unreserved|unreserved|
|CYCLE|unreserved|unreserved|
|DATA|unreserved|unreserved|
|DATABASE|unreserved|unreserved|
|DAY|unreserved|unreserved|
|DEALLOCATE|unreserved|unreserved|
|DEC|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|DECIMAL|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|DECLARE|unreserved|unreserved|
|DECODE|reserved||
|DEFAULT|reserved|reserved|
|DEFAULTS|unreserved|unreserved|
|DEFERRABLE|reserved|reserved|
|DEFERRED|unreserved|unreserved|
|DEFINER|unreserved|unreserved|
|DELETE|unreserved|unreserved|
|DELIMITER|unreserved|unreserved|
|DELIMITERS|unreserved|unreserved|
|DENY|unreserved||
|DEPENDS|unreserved|unreserved|
|DESC|reserved|reserved|
|DICTIONARY|unreserved|unreserved|
|DISABLE|unreserved|unreserved|
|DISCARD|unreserved|unreserved|
|DISTINCT|reserved|reserved|
|DISTRIBUTED|reserved||
|DO|reserved|reserved|
|DOCUMENT|unreserved|unreserved|
|DOMAIN|unreserved|unreserved|
|DOUBLE|unreserved|unreserved|
|DROP|unreserved|unreserved|
|DXL|unreserved||
|EACH|unreserved|unreserved|
|ELSE|reserved|reserved|
|ENABLE|unreserved|unreserved|
|ENCODING|unreserved|unreserved|
|ENCRYPTED|unreserved|unreserved|
|END|reserved|reserved|
|ENDPOINT|unreserved|unreserved|
|ENUM|unreserved|unreserved|
|ERRORS|unreserved||
|ESCAPE|unreserved|unreserved|
|EVENT|unreserved|unreserved|
|EVERY|unreserved||
|EXCEPT|reserved|reserved|
|EXCHANGE|unreserved||
|EXCLUDE|reserved|unreserved|
|EXCLUDING|unreserved|unreserved|
|EXCLUSIVE|unreserved|unreserved|
|EXECUTE|unreserved|unreserved|
|EXISTS|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|EXPAND|unreserved||
|EXPLAIN|unreserved|unreserved|
|EXTENSION|unreserved|unreserved|
|EXTERNAL|unreserved|unreserved|
|EXTRACT|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|FALSE|reserved|reserved|
|FAMILY|unreserved|unreserved|
|FETCH|reserved|reserved|
|FIELDS|unreserved||
|FILESPACE|unreserved|unreserved|
|FILL|unreserved||
|FILTER|unreserved|unreserved|
|FIRST|unreserved|unreserved|
|FLOAT|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|FOLLOWING|reserved|unreserved|
|FOR|reserved|reserved|
|FORCE|unreserved|unreserved|
|FOREIGN|reserved|reserved|
|FORMAT|unreserved||
|FORWARD|unreserved|unreserved|
|FREEZE|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|FROM|reserved|reserved|
|FULL|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|FULLSCAN|unreserved||
|FUNCTION|unreserved|unreserved|
|FUNCTIONS|unreserved|unreserved|
|GLOBAL|unreserved|unreserved|
|GRANT|reserved|reserved|
|GRANTED|unreserved|unreserved|
|GREATEST|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|GROUP|reserved|reserved|
|GROUP\_ID|unreserved \(cannot be function or type name\)||
|GROUPING|unreserved \(cannot be function or type name\)||
|HANDLER|unreserved|unreserved|
|HASH|unreserved||
|HAVING|reserved|reserved|
|HEADER|unreserved|unreserved|
|HOLD|unreserved|unreserved|
|HOST|unreserved||
|HOUR|unreserved|unreserved|
|IDENTITY|unreserved|unreserved|
|IF|unreserved|unreserved|
|IGNORE|unreserved||
|ILIKE|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|IMMEDIATE|unreserved|unreserved|
|IMMUTABLE|unreserved|unreserved|
|IMPLICIT|unreserved|unreserved|
|IMPORT|unreserved|unreserved|
|IN|reserved|reserved|
|INCLUDING|unreserved|unreserved|
|INCLUSIVE|unreserved||
|INCREMENT|unreserved|unreserved|
|INDEX|unreserved|unreserved|
|INDEXES|unreserved|unreserved|
|INHERIT|unreserved|unreserved|
|INHERITS|unreserved|unreserved|
|INITIALLY|reserved|reserved|
|INITPLAN|unreserved|unreserved|
|INLINE|unreserved|unreserved|
|INNER|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|INOUT|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|INPUT|unreserved|unreserved|
|INSENSITIVE|unreserved|unreserved|
|INSERT|unreserved|unreserved|
|INSTEAD|unreserved|unreserved|
|INT|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|INTEGER|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|INTERSECT|reserved|reserved|
|INTERVAL|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|INTO|reserved|reserved|
|INVOKER|unreserved|unreserved|
|IS|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|ISNULL|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|ISOLATION|unreserved|unreserved|
|JOIN|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|KEY|unreserved|unreserved|
|LABEL|unreserved|unreserved|
|LANGUAGE|unreserved|unreserved|
|LARGE|unreserved|unreserved|
|LAST|unreserved|unreserved|
|LATERAL|reserved|reserved|
|LC\_COLLATE|unreserved|unreserved|
|LC\_CTYPE|unreserved|unreserved|
|LEADING|reserved|reserved|
|LEAKPROOF|unreserved|unreserved|
|LEAST|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|LEFT|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|LEVEL|unreserved|unreserved|
|LIKE|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|LIMIT|reserved|reserved|
|LIST|unreserved||
|LISTEN|unreserved|unreserved|
|LOAD|unreserved|unreserved|
|LOCAL|unreserved|unreserved|
|LOCALTIME|reserved|reserved|
|LOCALTIMESTAMP|reserved|reserved|
|LOCATION|unreserved|unreserved|
|LOCK|unreserved|unreserved|
|LOCKED|unreserved|unreserved|
|LOG|reserved \(can be function or type name\)||
|LOGGED|unreserved|unreserved|
|MAPPING|unreserved|unreserved|
|MASTER|unreserved||
|MATCH|unreserved|unreserved|
|MATERIALIZED|unreserved|unreserved|
|MAXVALUE|unreserved|unreserved|
|MEDIAN|unreserved \(cannot be function or type name\)||
|MEMORY\_LIMIT|unreserved||
|MEMORY\_SHARED\_QUOTA|unreserved||
|MEMORY\_SPILL\_RATIO|unreserved||
|METHOD|unreserved|unreserved|
|MINUTE|unreserved|unreserved|
|MINVALUE|unreserved|unreserved|
|MISSING|unreserved||
|MODE|unreserved|unreserved|
|MODIFIES|unreserved||
|MONTH|unreserved|unreserved|
|MOVE|unreserved|unreserved|
|NAME|unreserved|unreserved|
|NAMES|unreserved|unreserved|
|NATIONAL|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|NATURAL|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|NCHAR|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|NEWLINE|unreserved||
|NEXT|unreserved|unreserved|
|NO|unreserved|unreserved|
|NOCREATEEXTTABLE|unreserved||
|NONE|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|NOOVERCOMMIT|unreserved||
|NOT|reserved|reserved|
|NOTHING|unreserved|unreserved|
|NOTIFY|unreserved|unreserved|
|NOTNULL|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|NOWAIT|unreserved|unreserved|
|NULL|reserved|reserved|
|NULLIF|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|NULLS|unreserved|unreserved|
|NUMERIC|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|OBJECT|unreserved|unreserved|
|OF|unreserved|unreserved|
|OFF|unreserved|unreserved|
|OFFSET|reserved|reserved|
|OIDS|unreserved|unreserved|
|ON|reserved|reserved|
|ONLY|reserved|reserved|
|OPERATOR|unreserved|unreserved|
|OPTION|unreserved|unreserved|
|OPTIONS|unreserved|unreserved|
|OR|reserved|reserved|
|ORDER|reserved|reserved|
|ORDERED|unreserved||
|ORDINALITY|unreserved|unreserved|
|OTHERS|unreserved||
|OUT|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|OUTER|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|OVER|unreserved|unreserved|
|OVERCOMMIT|unreserved||
|OVERLAPS|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|OVERLAY|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|OWNED|unreserved|unreserved|
|OWNER|unreserved|unreserved|
|PARALLEL|unreserved|unreserved|
|PARSER|unreserved|unreserved|
|PARTIAL|unreserved|unreserved|
|PARTITION|reserved|unreserved|
|PARTITIONS|unreserved||
|PASSING|unreserved|unreserved|
|PASSWORD|unreserved|unreserved|
|PERCENT|unreserved||
|PLACING|reserved|reserved|
|PLANS|unreserved|unreserved|
|POLICY|unreserved|unreserved|
|POSITION|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|PRECEDING|reserved|unreserved|
|PRECISION|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|PREPARE|unreserved|unreserved|
|PREPARED|unreserved|unreserved|
|PRESERVE|unreserved|unreserved|
|PRIMARY|reserved|reserved|
|PRIOR|unreserved|unreserved|
|PRIVILEGES|unreserved|unreserved|
|PROCEDURAL|unreserved|unreserved|
|PROCEDURE|unreserved|unreserved|
|PROGRAM|unreserved|unreserved|
|PROTOCOL|unreserved||
|QUEUE|unreserved||
|QUOTE|unreserved|unreserved|
|RANDOMLY|unreserved||
|RANGE|unreserved|unreserved|
|READ|unreserved|unreserved|
|READABLE|unreserved||
|READS|unreserved||
|REAL|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|REASSIGN|unreserved|unreserved|
|RECHECK|unreserved|unreserved|
|RECURSIVE|unreserved|unreserved|
|REF|unreserved|unreserved|
|REFERENCES|reserved|reserved|
|REFRESH|unreserved|unreserved|
|REINDEX|unreserved|unreserved|
|REJECT|unreserved||
|RELATIVE|unreserved|unreserved|
|RELEASE|unreserved|unreserved|
|RENAME|unreserved|unreserved|
|REPEATABLE|unreserved|unreserved|
|REPLACE|unreserved|unreserved|
|REPLICA|unreserved|unreserved|
|REPLICATED|unreserved||
|RESET|unreserved|unreserved|
|RESOURCE|unreserved||
|RESTART|unreserved|unreserved|
|RESTRICT|unreserved|unreserved|
|RETRIEVE|unreserved|unreserved|
|RETURNING|reserved|reserved|
|RETURNS|unreserved|unreserved|
|REVOKE|unreserved|unreserved|
|RIGHT|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|ROLE|unreserved|unreserved|
|ROLLBACK|unreserved|unreserved|
|ROLLUP|unreserved \(cannot be function or type name\)||
|ROOTPARTITION|unreserved||
|ROW|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|ROWS|unreserved|unreserved|
|RULE|unreserved|unreserved|
|SAVEPOINT|unreserved|unreserved|
|SCATTER|reserved||
|SCHEMA|unreserved|unreserved|
|SCROLL|unreserved|unreserved|
|SEARCH|unreserved|unreserved|
|SECOND|unreserved|unreserved|
|SECURITY|unreserved|unreserved|
|SEGMENT|unreserved||
|SEGMENTS|unreserved||
|SELECT|reserved|reserved|
|SEQUENCE|unreserved|unreserved|
|SEQUENCES|unreserved|unreserved|
|SERIALIZABLE|unreserved|unreserved|
|SERVER|unreserved|unreserved|
|SESSION|unreserved|unreserved|
|SESSION\_USER|reserved|reserved|
|SET|unreserved|unreserved|
|SETOF|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|SETS|unreserved \(cannot be function or type name\)||
|SHARE|unreserved|unreserved|
|SHOW|unreserved|unreserved|
|SIMILAR|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|SIMPLE|unreserved|unreserved|
|SKIP|unreserved|unreserved|
|SMALLINT|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|SNAPSHOT|unreserved|unreserved|
|SOME|reserved|reserved|
|SPLIT|unreserved||
|SQL|unreserved||
|STABLE|unreserved|unreserved|
|STANDALONE|unreserved|unreserved|
|START|unreserved|unreserved|
|STATEMENT|unreserved|unreserved|
|STATISTICS|unreserved|unreserved|
|STDIN|unreserved|unreserved|
|STDOUT|unreserved|unreserved|
|STORAGE|unreserved|unreserved|
|STRICT|unreserved|unreserved|
|STRIP|unreserved|unreserved|
|SUBPARTITION|unreserved||
|SUBSTRING|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|SYMMETRIC|reserved|reserved|
|SYSID|unreserved|unreserved|
|SYSTEM|unreserved|unreserved|
|TABLE|reserved|reserved|
|TABLES|unreserved|unreserved|
|TABLESPACE|unreserved|unreserved|
|TEMP|unreserved|unreserved|
|TEMPLATE|unreserved|unreserved|
|TEMPORARY|unreserved|unreserved|
|TEXT|unreserved|unreserved|
|THEN|reserved|reserved|
|THRESHOLD|unreserved||
|TIES|unreserved||
|TIME|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|TIMESTAMP|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|TO|reserved|reserved|
|TRAILING|reserved|reserved|
|TRANSACTION|unreserved|unreserved|
|TRANSFORM|unreserved|unreserved|
|TREAT|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|TRIGGER|unreserved|unreserved|
|TRIM|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|TRUE|reserved|reserved|
|TRUNCATE|unreserved|unreserved|
|TRUSTED|unreserved|unreserved|
|TYPE|unreserved|unreserved|
|TYPES|unreserved|unreserved|
|UNBOUNDED|reserved|unreserved|
|UNCOMMITTED|unreserved|unreserved|
|UNENCRYPTED|unreserved|unreserved|
|UNION|reserved|reserved|
|UNIQUE|reserved|reserved|
|UNKNOWN|unreserved|unreserved|
|UNLISTEN|unreserved|unreserved|
|UNLOGGED|unreserved|unreserved|
|UNTIL|unreserved|unreserved|
|UPDATE|unreserved|unreserved|
|USER|reserved|reserved|
|USING|reserved|reserved|
|VACUUM|unreserved|unreserved|
|VALID|unreserved|unreserved|
|VALIDATE|unreserved|unreserved|
|VALIDATION|unreserved||
|VALIDATOR|unreserved|unreserved|
|VALUE|unreserved|unreserved|
|VALUES|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|VARCHAR|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|VARIADIC|reserved|reserved|
|VARYING|unreserved|unreserved|
|VERBOSE|reserved \(can be function or type name\)|reserved \(can be function or type name\)|
|VERSION|unreserved|unreserved|
|VIEW|unreserved|unreserved|
|VIEWS|unreserved|unreserved|
|VOLATILE|unreserved|unreserved|
|WEB|unreserved||
|WHEN|reserved|reserved|
|WHERE|reserved|reserved|
|WHITESPACE|unreserved|unreserved|
|WINDOW|reserved|reserved|
|WITH|reserved|reserved|
|WITHIN|unreserved|unreserved|
|WITHOUT|unreserved|unreserved|
|WORK|unreserved|unreserved|
|WRAPPER|unreserved|unreserved|
|WRITABLE|unreserved||
|WRITE|unreserved|unreserved|
|XML|unreserved|unreserved|
|XMLATTRIBUTES|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|XMLCONCAT|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|XMLELEMENT|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|XMLEXISTS|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|XMLFOREST|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|XMLPARSE|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|XMLPI|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|XMLROOT|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|XMLSERIALIZE|unreserved \(cannot be function or type name\)|unreserved \(cannot be function or type name\)|
|YEAR|unreserved|unreserved|
|YES|unreserved|unreserved|
|ZONE|unreserved|unreserved|

