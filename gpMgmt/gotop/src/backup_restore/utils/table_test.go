package utils_test

import (
	"backup_restore/utils"
	"database/sql"
	"fmt"
	"testing"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
)

func TestTable(t *testing.T) {
	RegisterFailHandler(Fail)
}

var _ = Describe("utils/table tests", func() {
	Describe("QuoteIdent", func() {
		It("does not quote a string that contains no special characters", func() {
			name := `_tablename1`
			output := utils.QuoteIdent(name)
			Expect(output).To(Equal(`_tablename1`))
		})
		It("replaces double quotes with pairs of double quotes", func() {
			name := `table"name`
			output := utils.QuoteIdent(name)
			Expect(output).To(Equal(`"table""name"`))
		})
		It("properly escapes capital letters", func() {
			names := []string{"Tablename", "TABLENAME", "TaBlEnAmE"}
			expected := []string{`"Tablename"`, `"TABLENAME"`, `"TaBlEnAmE"`}
			for i := range names {
				output := utils.QuoteIdent(names[i])
				Expect(output).To(Equal(expected[i]))
			}
		})
		It("properly escapes shell-significant special characters", func() {
			special := `.,!$/\` + "`"
			for _, spec := range special {
				name := fmt.Sprintf(`table%cname`, spec)
				expected := fmt.Sprintf(`"table%cname"`, spec)
				output := utils.QuoteIdent(name)
				Expect(output).To(Equal(expected))
			}
		})
		It("properly escapes whitespace", func() {
			names := []string{"table name", "table\tname", "table\nname"}
			expected := []string{`"table name"`, `"table\tname"`, `"table\nname"`}
			for i := range names {
				output := utils.QuoteIdent(names[i])
				Expect(output).To(Equal(expected[i]))
			}
		})
		It("properly escapes all other punctuation", func() {
			special := `'~@#$%^&*()-+[]{}><|;:?`
			for _, spec := range special {
				name := fmt.Sprintf(`table%cname`, spec)
				expected := fmt.Sprintf(`"table%cname"`, spec)
				output := utils.QuoteIdent(name)
				Expect(output).To(Equal(expected))
			}
		})
		It("properly escapes Unicode characters", func() {
			special := `Ää表`
			for _, spec := range special {
				name := fmt.Sprintf(`table%cname`, spec)
				expected := fmt.Sprintf(`"table%cname"`, spec)
				output := utils.QuoteIdent(name)
				Expect(output).To(Equal(expected))
			}
		})
	})
	Describe("DBObject.ToString", func() {
		It("remains unquoted if it contains no special characters", func() {
			testSchema := utils.DBObject{0, `schemaname`, sql.NullString{"", false}}
			expected := `schemaname`
			Expect(testSchema.ToString()).To(Equal(expected))
		})
		It("is quoted if it contains special characters", func() {
			testSchema := utils.DBObject{0, `schema,name`, sql.NullString{"", false}}
			expected := `"schema,name"`
			Expect(testSchema.ToString()).To(Equal(expected))
		})
	})
	Describe("DBObjectFromString", func() {
		It("can parse an unquoted string", func() {
			testString := `schemaname`
			newSchema := utils.DBObjectFromString(testString)
			Expect(newSchema.ObjOid).To(Equal(uint32(0)))
			Expect(newSchema.ObjName).To(Equal(`schemaname`))
		})
		It("can parse a quoted string", func() {
			testString := `"schema,name"`
			newSchema := utils.DBObjectFromString(testString)
			Expect(newSchema.ObjOid).To(Equal(uint32(0)))
			Expect(newSchema.ObjName).To(Equal(`schema,name`))
		})
	})
	Describe("Table.ToString", func() {
		It("remains unquoted if neither the schema nor the table name contains special characters", func() {
			testTable := utils.Table{0, 0, `schemaname`, `tablename`}
			expected := `schemaname.tablename`
			Expect(testTable.ToString()).To(Equal(expected))
		})
		It("is quoted if the schema name contains special characters", func() {
			testTable := utils.Table{0, 0, `schema,name`, `tablename`}
			expected := `"schema,name".tablename`
			Expect(testTable.ToString()).To(Equal(expected))
		})
		It("is quoted if the table name contains special characters", func() {
			testTable := utils.Table{0, 0, `schemaname`, `table,name`}
			expected := `schemaname."table,name"`
			Expect(testTable.ToString()).To(Equal(expected))
		})
		It("is quoted if both the schema and the table name contain special characters", func() {
			testTable := utils.Table{0, 0, `schema,name`, `table,name`}
			expected := `"schema,name"."table,name"`
			Expect(testTable.ToString()).To(Equal(expected))
		})
	})
	Describe("TableFromString", func() {
		It("can parse an unquoted string", func() {
			testString := `schemaname.tablename`
			newTable := utils.TableFromString(testString)
			Expect(newTable.SchemaOid).To(Equal(uint32(0)))
			Expect(newTable.TableOid).To(Equal(uint32(0)))
			Expect(newTable.SchemaName).To(Equal(`schemaname`))
			Expect(newTable.TableName).To(Equal(`tablename`))
		})
		It("can parse a string with a quoted schema", func() {
			testString := `"schema,name".tablename`
			newTable := utils.TableFromString(testString)
			Expect(newTable.SchemaOid).To(Equal(uint32(0)))
			Expect(newTable.TableOid).To(Equal(uint32(0)))
			Expect(newTable.SchemaName).To(Equal(`schema,name`))
			Expect(newTable.TableName).To(Equal(`tablename`))
		})
		It("can parse a string with a quoted table", func() {
			testString := `schemaname."table,name"`
			newTable := utils.TableFromString(testString)
			Expect(newTable.SchemaOid).To(Equal(uint32(0)))
			Expect(newTable.TableOid).To(Equal(uint32(0)))
			Expect(newTable.SchemaName).To(Equal(`schemaname`))
			Expect(newTable.TableName).To(Equal(`table,name`))
		})
		It("can parse a string with both schema and table quoted", func() {
			testString := `"schema,name"."table,name"`
			newTable := utils.TableFromString(testString)
			Expect(newTable.SchemaOid).To(Equal(uint32(0)))
			Expect(newTable.TableOid).To(Equal(uint32(0)))
			Expect(newTable.SchemaName).To(Equal(`schema,name`))
			Expect(newTable.TableName).To(Equal(`table,name`))
		})
	})
	Describe("GetUniqueSchemas", func() {
		alphabeticalAFoo := utils.Table{1, 0, "otherschema", "foo"}
		alphabeticalABar := utils.Table{1, 0, "otherschema", "bar"}
		schemaOther := utils.DBObject{2, "otherschema", sql.NullString{"", false}}
		alphabeticalBFoo := utils.Table{2, 0, "public", "foo"}
		alphabeticalBBar := utils.Table{2, 0, "public", "bar"}
		schemaPublic := utils.DBObject{1, "public", sql.NullString{"Standard public schema", true}}
		schemas := []utils.DBObject{schemaOther, schemaPublic}

		It("has multiple tables in a single schema", func() {
			tables := []utils.Table{alphabeticalAFoo, alphabeticalABar}
			uniqueSchemas := utils.GetUniqueSchemas(schemas, tables)
			Expect(uniqueSchemas).To(Equal([]utils.DBObject{schemaPublic}))
		})
		It("has multiple schemas, each with multiple tables", func() {
			tables := []utils.Table{alphabeticalBFoo, alphabeticalBBar, alphabeticalAFoo, alphabeticalABar}
			uniqueSchemas := utils.GetUniqueSchemas(schemas, tables)
			Expect(uniqueSchemas).To(Equal([]utils.DBObject{schemaOther, schemaPublic}))
		})
		It("has no tables", func() {
			tables := []utils.Table{}
			uniqueSchemas := utils.GetUniqueSchemas(schemas, tables)
			Expect(uniqueSchemas).To(Equal([]utils.DBObject{}))
		})
	})
})
