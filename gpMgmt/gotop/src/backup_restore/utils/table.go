package utils

import (
	"database/sql"
	"fmt"
	"regexp"
	"sort"
	"strings"
)

var (
	/* To be used in a Postgres query without being quoted, an identifier (schema or
	 * table) must begin with a lowercase letter or underscore, and may contain only
	 * lowercast letters, digits, and underscores.
	 */
	unquotedIdentifier = regexp.MustCompile(`^([a-z_][a-z0-9_]*)$`)
	quotedIdentifier   = regexp.MustCompile(`^"(.+)"$`)

	quotedOrUnquotedString = regexp.MustCompile(`^(?:\"(.*)\"|(.*))\.(?:\"(.*)\"|(.*))$`)

	// Swap between double quotes and paired double quotes, and between literal whitespace characters and escape sequences
	replacerTo = strings.NewReplacer("\"", "\"\"", `
`, "\\n", "\n", "\\n", "	", "\\t", "\t", "\\t")
	replacerFrom = strings.NewReplacer("\"\"", "\"", "\\n", "\n", "\\t", "\t")
)

// This will mostly be used for schemas, but can be used for any database object with an oid.
type DBObject struct {
	ObjOid     uint32
	ObjName    string
	ObjComment sql.NullString
}

type Table struct {
	SchemaOid  uint32
	TableOid   uint32
	SchemaName string
	TableName  string
}

/*
 * Functions for escaping schemas and tables
 */

// Quote an unquoted identifier (schema or table) like quote_ident in Postgres
func QuoteIdent(ident string) string {
	if !unquotedIdentifier.MatchString(ident) {
		ident = replacerTo.Replace(ident)
		ident = fmt.Sprintf(`"%s"`, ident)
	}
	return ident
}

func (s DBObject) ToString() string {
	return QuoteIdent(s.ObjName)
}

// Print a table in schema.table format, with everything escaped appropriately.
func (t Table) ToString() string {
	schema := QuoteIdent(t.SchemaName)
	table := QuoteIdent(t.TableName)
	return fmt.Sprintf("%s.%s", schema, table)
}

func DBObjectFromString(name string) DBObject {
	var object string
	var matches []string
	if matches = quotedIdentifier.FindStringSubmatch(name); len(matches) != 0 {
		object = replacerFrom.Replace(matches[1])
	} else if matches = unquotedIdentifier.FindStringSubmatch(name); len(matches) != 0 {
		object = replacerFrom.Replace(matches[1])
	} else {
		logger.Fatal("\"%s\" is not a valid identifier", name)
	}
	return DBObject{0, object, sql.NullString{"", false}}
}

/* Parse an appropriately-escaped schema.table string into a Table.  The Table's
 * Oid fields are left at 0, and will need to be filled in with the real values
 * if the Table is to be used in any Get[Something]() function in queries.go.
 */
func TableFromString(name string) Table {
	var schema, table string
	var matches []string
	if matches = quotedOrUnquotedString.FindStringSubmatch(name); len(matches) != 0 {
		if matches[1] != "" { // schema was quoted
			schema = replacerFrom.Replace(matches[1])
		} else { // schema wasn't quoted
			schema = replacerFrom.Replace(matches[2])
		}
		if matches[3] != "" { // table was quoted
			table = replacerFrom.Replace(matches[3])
		} else { // table wasn't quoted
			table = replacerFrom.Replace(matches[4])
		}
	} else {
		logger.Fatal("\"%s\" is not a valid fully-qualified table expression", name)
	}
	return Table{0, 0, schema, table}
}

/*
 * Functions for sorting schemas and tables
 */

type DBObjects []DBObject

func (slice DBObjects) Len() int {
	return len(slice)
}

func (slice DBObjects) Less(i int, j int) bool {
	return slice[i].ObjName < slice[j].ObjName
}

func (slice DBObjects) Swap(i int, j int) {
	slice[i], slice[j] = slice[j], slice[i]
}

func SortDBObjects(objects DBObjects) {
	sort.Sort(objects)
}

type Tables []Table

func (slice Tables) Len() int {
	return len(slice)
}

func (slice Tables) Less(i int, j int) bool {
	if slice[i].SchemaName < slice[j].SchemaName {
		return true
	}
	if slice[i].TableName < slice[j].TableName {
		return true
	}
	return false
}

func (slice Tables) Swap(i int, j int) {
	slice[i], slice[j] = slice[j], slice[i]
}

func SortTables(tables Tables) {
	sort.Sort(tables)
}

/*
 * Other schema- and table-related utility functions
 */

// Given a list of Tables, returns a sorted list of their Schemas.
// Assumes that the Table list is sorted by schema and then by table.
func GetUniqueSchemas(schemas []DBObject, tables []Table) []DBObject {
	currentSchemaOid := uint32(0)
	uniqueSchemas := make([]DBObject, 0)
	schemaMap := make(map[uint32]DBObject, 0)
	for _, schema := range schemas {
		schemaMap[schema.ObjOid] = schema
	}
	for _, table := range tables {
		if table.SchemaOid != currentSchemaOid {
			currentSchemaOid = table.SchemaOid
			uniqueSchemas = append(uniqueSchemas, schemaMap[currentSchemaOid])
		}
	}
	return uniqueSchemas
}
