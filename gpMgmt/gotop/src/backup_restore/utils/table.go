package utils

import (
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
	quotedIdentifier  = regexp.MustCompile(`^"(.+)"$`)

	quotedOrUnquotedString = regexp.MustCompile(`^(?:\"(.*)\"|(.*))\.(?:\"(.*)\"|(.*))$`)

	replacerTo   = strings.NewReplacer("\"", "\"\"", "\n", "\\n", "\t", "\\t")
	replacerFrom = strings.NewReplacer("\"\"", "\"", "\\n", "\n", "\\t", "\t")
)

type Schema struct {
	SchemaOid  uint32
	SchemaName string
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

func (s Schema) ToString() string {
	return QuoteIdent(s.SchemaName)
}

// Print a table in schema.table format, with everything escaped appropriately.
func (t Table) ToString() string {
	schema := QuoteIdent(t.SchemaName)
	table := QuoteIdent(t.TableName)
	return fmt.Sprintf("%s.%s", schema, table)
}

func SchemaFromString(name string) Schema {
	var schema string
	var matches []string
	if matches = quotedIdentifier.FindStringSubmatch(name); len(matches) != 0 {
		schema = replacerFrom.Replace(matches[1])
	} else if matches = unquotedIdentifier.FindStringSubmatch(name); len(matches) != 0 {
		schema = replacerFrom.Replace(matches[1])
	} else {
		logger.Fatal("\"%s\" is not a valid identifier", name)
	}
	return Schema{0, schema}
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

type Schemas []Schema

func (slice Schemas) Len() int {
	return len(slice)
}

func (slice Schemas) Less(i int, j int) bool {
	return slice[i].SchemaName < slice[j].SchemaName
}

func (slice Schemas) Swap(i int, j int) {
	slice[i], slice[j] = slice[j], slice[i]
}

func SortSchemas(schemas Schemas) {
	sort.Sort(schemas)
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

func SortTables(schemas Tables) {
	sort.Sort(schemas)
}

/*
 * Other schema- and table-related utility functions
 */

// Given a list of Tables, returns a sorted list of their Schemas.
func GetUniqueSchemas(tables []Table) []Schema {
	schemaMap := make(map[Schema]bool, 0)
	for _, table := range tables {
		schemaMap[Schema{table.SchemaOid, table.SchemaName}] = true
	}
	schemas := make([]Schema, 0)
	for schema := range schemaMap {
		if schema.SchemaName != "public" {
			schemas = append(schemas, schema)
		}
	}
	SortSchemas(schemas)
	return schemas
}
