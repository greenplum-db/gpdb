package backup_test

import (
	"backup_restore/backup"
	"backup_restore/testutils"
	"backup_restore/utils"
	"database/sql/driver"
	"errors"
	"testing"

	. "github.com/onsi/ginkgo"
	. "github.com/onsi/gomega"
	"gopkg.in/DATA-DOG/go-sqlmock.v1"
)

var connection *utils.DBConn
var mock sqlmock.Sqlmock

func TestQueries(t *testing.T) {
	RegisterFailHandler(Fail)
}

var _ = Describe("backup/queries tests", func() {
	BeforeSuite(func() {
		connection, mock = testutils.CreateAndConnectMockDB()
	})
	Describe("GetTableAttributes", func() {
		header := []string{"attname", "attnotnull", "atthasdef", "attisdropped", "atttypname", "attencoding"}
		rowOne := []driver.Value{"i", "f", "f", "f", "int", nil}
		rowTwo := []driver.Value{"j", "f", "f", "f", "character varying(20)", nil}
		rowEncoded := []driver.Value{"j", "f", "f", "f", "character varying(20)", "compresstype=zlib, blocksize=65536"}
		rowNotNull := []driver.Value{"j", "t", "f", "f", "character varying(20)", nil}

		It("returns a slice for a table with one column", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(rowOne...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetTableAttributes(connection, 0)
			Expect(len(results)).To(Equal(1))
			Expect(results[0].AttName).To(Equal("i"))
			Expect(results[0].AttHasDef).To(Equal(false))
			Expect(results[0].AttIsDropped).To(Equal(false))
		})
		It("returns a slice for a table with two columns", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(rowOne...).AddRow(rowTwo...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetTableAttributes(connection, 0)
			Expect(len(results)).To(Equal(2))
			Expect(results[0].AttName).To(Equal("i"))
			Expect(results[0].AttTypName).To(Equal("int"))
			Expect(results[1].AttName).To(Equal("j"))
			Expect(results[1].AttTypName).To(Equal("character varying(20)"))
		})
		It("returns a slice for a table with one NOT NULL column with ENCODING", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(rowOne...).AddRow(rowEncoded...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetTableAttributes(connection, 0)
			Expect(len(results)).To(Equal(2))
			Expect(results[0].AttName).To(Equal("i"))
			Expect(results[0].AttEncoding.Valid).To(Equal(false))
			Expect(results[1].AttName).To(Equal("j"))
			Expect(results[1].AttEncoding.Valid).To(Equal(true))
			Expect(results[1].AttEncoding.String).To(Equal("compresstype=zlib, blocksize=65536"))
		})
		It("returns a slice for a table with one NOT NULL column", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(rowOne...).AddRow(rowNotNull...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetTableAttributes(connection, 0)
			Expect(len(results)).To(Equal(2))
			Expect(results[0].AttName).To(Equal("i"))
			Expect(results[0].AttEncoding.Valid).To(Equal(false))
			Expect(results[1].AttName).To(Equal("j"))
			Expect(results[1].AttNotNull).To(Equal(true))
		})
		It("panics when table does not exist", func() {
			mock.ExpectQuery("SELECT (.*)").WillReturnError(errors.New("relation \"foo\" does not exist"))
			defer testutils.ShouldPanicWithMessage("relation \"foo\" does not exist")
			backup.GetTableAttributes(connection, 0)
		})
	})
	Describe("GetTableDefaults", func() {
		header := []string{"adnum", "defval"}
		rowOne := []driver.Value{"1", "42"}
		rowTwo := []driver.Value{"2", "bar"}

		It("returns a slice for a table with one column having a default value", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(rowOne...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetTableDefaults(connection, 0)
			Expect(len(results)).To(Equal(1))
			Expect(results[0].AdNum).To(Equal(1))
			Expect(results[0].DefVal).To(Equal("42"))
		})
		It("returns a slice for a table with two columns having default values", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(rowOne...).AddRow(rowTwo...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetTableDefaults(connection, 0)
			Expect(len(results)).To(Equal(2))
			Expect(results[0].AdNum).To(Equal(1))
			Expect(results[0].DefVal).To(Equal("42"))
			Expect(results[1].AdNum).To(Equal(2))
			Expect(results[1].DefVal).To(Equal("bar"))
		})
		It("returns a slice for a table with no columns having default values", func() {
			fakeResult := sqlmock.NewRows(header)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetTableDefaults(connection, 0)
			Expect(len(results)).To(Equal(0))
		})
	})
	Describe("GetConstraints", func() {
		header := []string{"conname", "condef"}
		rowOneUnique := []driver.Value{"tablename_i_uniq", "UNIQUE (i)"}
		rowTwoUnique := []driver.Value{"tablename_j_uniq", "UNIQUE (j)"}
		rowPrimarySingle := []driver.Value{"tablename_pkey", "PRIMARY KEY (i)"}
		rowPrimaryComposite := []driver.Value{"tablename_pkey", "PRIMARY KEY (i, j)"}
		rowOneForeign := []driver.Value{"tablename_i_fkey", "FOREIGN KEY (i) REFERENCES other_tablename(a)"}
		rowTwoForeign := []driver.Value{"tablename_j_fkey", "FOREIGN KEY (j) REFERENCES other_tablename(b)"}
		rowCheckSingle := []driver.Value{"check_i", "CHECK (i <> 42)"}
		rowCheckComposite := []driver.Value{"check_ij", "CHECK (i <> 42 AND j::text <> ''::text)"}

		Context("No constraints", func() {
			It("returns a slice for a table with no UNIQUE or PRIMARY KEY columns", func() {
				fakeResult := sqlmock.NewRows(header)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
				results := backup.GetConstraints(connection, 0)
				Expect(len(results)).To(Equal(0))
			})
		})
		Context("Columns with one constraint", func() {
			It("returns a slice for a table with one UNIQUE column", func() {
				fakeResult := sqlmock.NewRows(header).AddRow(rowOneUnique...)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
				results := backup.GetConstraints(connection, 0)
				Expect(len(results)).To(Equal(1))
				Expect(results[0].ConName).To(Equal("tablename_i_uniq"))
				Expect(results[0].ConDef).To(Equal("UNIQUE (i)"))
			})
			It("returns a slice for a table with two UNIQUE columns", func() {
				fakeResult := sqlmock.NewRows(header).AddRow(rowOneUnique...).AddRow(rowTwoUnique...)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
				results := backup.GetConstraints(connection, 0)
				Expect(len(results)).To(Equal(2))
				Expect(results[0].ConName).To(Equal("tablename_i_uniq"))
				Expect(results[0].ConDef).To(Equal("UNIQUE (i)"))
				Expect(results[1].ConName).To(Equal("tablename_j_uniq"))
				Expect(results[1].ConDef).To(Equal("UNIQUE (j)"))
			})
			It("returns a slice for a table with a PRIMARY KEY on one column", func() {
				fakeResult := sqlmock.NewRows(header).AddRow(rowPrimarySingle...)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
				results := backup.GetConstraints(connection, 0)
				Expect(len(results)).To(Equal(1))
				Expect(results[0].ConName).To(Equal("tablename_pkey"))
				Expect(results[0].ConDef).To(Equal("PRIMARY KEY (i)"))
			})
			It("returns a slice for a table with a composite PRIMARY KEY on two columns", func() {
				fakeResult := sqlmock.NewRows(header).AddRow(rowPrimaryComposite...)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
				results := backup.GetConstraints(connection, 0)
				Expect(len(results)).To(Equal(1))
				Expect(results[0].ConName).To(Equal("tablename_pkey"))
				Expect(results[0].ConDef).To(Equal("PRIMARY KEY (i, j)"))
			})
			It("returns a slice for a table with one FOREIGN KEY column", func() {
				fakeResult := sqlmock.NewRows(header).AddRow(rowOneForeign...)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
				results := backup.GetConstraints(connection, 0)
				Expect(len(results)).To(Equal(1))
				Expect(results[0].ConName).To(Equal("tablename_i_fkey"))
				Expect(results[0].ConDef).To(Equal("FOREIGN KEY (i) REFERENCES other_tablename(a)"))
			})
			It("returns a slice for a table with two FOREIGN KEY columns", func() {
				fakeResult := sqlmock.NewRows(header).AddRow(rowOneForeign...).AddRow(rowTwoForeign...)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
				results := backup.GetConstraints(connection, 0)
				Expect(len(results)).To(Equal(2))
				Expect(results[0].ConName).To(Equal("tablename_i_fkey"))
				Expect(results[0].ConDef).To(Equal("FOREIGN KEY (i) REFERENCES other_tablename(a)"))
				Expect(results[1].ConName).To(Equal("tablename_j_fkey"))
				Expect(results[1].ConDef).To(Equal("FOREIGN KEY (j) REFERENCES other_tablename(b)"))
			})
			It("returns a slice for a table with a CHECK on one column", func() {
				fakeResult := sqlmock.NewRows(header).AddRow(rowCheckSingle...)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
				results := backup.GetConstraints(connection, 0)
				Expect(len(results)).To(Equal(1))
				Expect(results[0].ConName).To(Equal("check_i"))
				Expect(results[0].ConDef).To(Equal("CHECK (i <> 42)"))
			})
			It("returns a slice for a table with a composite CHECK on two columns", func() {
				fakeResult := sqlmock.NewRows(header).AddRow(rowCheckComposite...)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
				results := backup.GetConstraints(connection, 0)
				Expect(len(results)).To(Equal(1))
				Expect(results[0].ConName).To(Equal("check_ij"))
				Expect(results[0].ConDef).To(Equal("CHECK (i <> 42 AND j::text <> ''::text)"))
			})
		})
		Context("Tables with multiple constraints", func() {
			It("returns a slice for a table with one column having each simple constraint type", func() {
				fakeResult := sqlmock.NewRows(header).AddRow(rowOneUnique...).AddRow(rowOneForeign...).AddRow(rowCheckSingle...)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
				results := backup.GetConstraints(connection, 0)
				Expect(len(results)).To(Equal(3))
				Expect(results[0].ConName).To(Equal("tablename_i_uniq"))
				Expect(results[0].ConDef).To(Equal("UNIQUE (i)"))
				Expect(results[1].ConName).To(Equal("tablename_i_fkey"))
				Expect(results[1].ConDef).To(Equal("FOREIGN KEY (i) REFERENCES other_tablename(a)"))
				Expect(results[2].ConName).To(Equal("check_i"))
				Expect(results[2].ConDef).To(Equal("CHECK (i <> 42)"))
			})
			It("returns a slice for a table with one column having each complex constraint type", func() {
				fakeResult := sqlmock.NewRows(header).AddRow(rowOneForeign...).AddRow(rowPrimaryComposite...).AddRow(rowCheckComposite...)
				mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
				results := backup.GetConstraints(connection, 0)
				Expect(len(results)).To(Equal(3))
				Expect(results[0].ConName).To(Equal("tablename_i_fkey"))
				Expect(results[0].ConDef).To(Equal("FOREIGN KEY (i) REFERENCES other_tablename(a)"))
				Expect(results[1].ConName).To(Equal("tablename_pkey"))
				Expect(results[1].ConDef).To(Equal("PRIMARY KEY (i, j)"))
				Expect(results[2].ConName).To(Equal("check_ij"))
				Expect(results[2].ConDef).To(Equal("CHECK (i <> 42 AND j::text <> ''::text)"))
			})
		})
	})
	Describe("GetDistributionPolicy", func() {
		header := []string{"attname"}
		rowDistOne := []driver.Value{"i"}
		rowDistTwo := []driver.Value{"j"}

		It("returns a slice for a table DISTRIBUTED RANDOMLY", func() {
			fakeResult := sqlmock.NewRows(header)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetDistributionPolicy(connection, 0)
			Expect(results).To(Equal("DISTRIBUTED RANDOMLY"))
		})
		It("returns a slice for a table DISTRIBUTED BY one column", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(rowDistOne...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetDistributionPolicy(connection, 0)
			Expect(results).To(Equal("DISTRIBUTED BY (i)"))
		})
		It("returns a slice for a table DISTRIBUTED BY two columns", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(rowDistOne...).AddRow(rowDistTwo...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetDistributionPolicy(connection, 0)
			Expect(results).To(Equal("DISTRIBUTED BY (i, j)"))
		})
	})
	Describe("GetPartitionDefinition", func() {
		header := []string{"partitiondef"}
		partRow := []driver.Value{`PARTITION BY RANGE(year)
	(
	START (2001) END (2002) EVERY (1) WITH (tablename='rank_1_prt_2', appendonly=false ),
	START (2002) END (2003) EVERY (1) WITH (tablename='rank_1_prt_3', appendonly=false ),
	DEFAULT PARTITION extra  WITH (tablename='rank_1_prt_extra', appendonly=false )
	);`}

		It("returns a partition definition for a partition table", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(partRow...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			result := backup.GetPartitionDefinition(connection, 0)
			Expect(result).To(Equal(`PARTITION BY RANGE(year)
	(
	START (2001) END (2002) EVERY (1) WITH (tablename='rank_1_prt_2', appendonly=false ),
	START (2002) END (2003) EVERY (1) WITH (tablename='rank_1_prt_3', appendonly=false ),
	DEFAULT PARTITION extra  WITH (tablename='rank_1_prt_extra', appendonly=false )
	);`))
		})
		It("returns empty string for a non-partition table", func() {
			fakeResult := sqlmock.NewRows(header)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			result := backup.GetPartitionDefinition(connection, 0)
			Expect(result).To(Equal(""))
		})
	})
	Describe("GetPartitionTemplateDefinition", func() {
		header := []string{"partitiondef"}
		partRow := []driver.Value{`ALTER TABLE tablename
SET SUBPARTITION TEMPLATE
          (
          SUBPARTITION usa VALUES('usa') WITH (tablename='tablename'),
          SUBPARTITION asia VALUES('asia') WITH (tablename='tablename'),
          SUBPARTITION europe VALUES('europe') WITH (tablename='tablename'),
          DEFAULT SUBPARTITION other_regions  WITH (tablename='tablename')
          )
`}

		It("returns a subpartition template definition for a multi-level partition table", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(partRow...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			result := backup.GetPartitionTemplateDefinition(connection, 0)
			Expect(result).To(Equal(`ALTER TABLE tablename
SET SUBPARTITION TEMPLATE
          (
          SUBPARTITION usa VALUES('usa') WITH (tablename='tablename'),
          SUBPARTITION asia VALUES('asia') WITH (tablename='tablename'),
          SUBPARTITION europe VALUES('europe') WITH (tablename='tablename'),
          DEFAULT SUBPARTITION other_regions  WITH (tablename='tablename')
          )
`))
		})
		It("returns empty string for a non-partition table", func() {
			fakeResult := sqlmock.NewRows(header)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			result := backup.GetPartitionTemplateDefinition(connection, 0)
			Expect(result).To(Equal(""))
		})
	})
	Describe("GetStorageOptions", func() {
		header := []string{"storageoptions"}
		rowAo := []driver.Value{"(appendonly=true)"}
		rowCo := []driver.Value{"(appendonly=true, orientation=column)"}
		rowFill := []driver.Value{"(fillfactor=42)"}
		rowAoFill := []driver.Value{"(appendonly=true, fillfactor=42)"}
		rowCoFill := []driver.Value{"(appendonly=true, orientation=column, fillfactor=42)"}
		rowCoMany := []driver.Value{"(appendonly=true, orientation=column, fillfactor=42, compresstype=zlib, blocksize=32768, compresslevel=1)"}

		It("returns a slice for an append-optimized table", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(rowAo...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetStorageOptions(connection, 0)
			Expect(results).To(Equal("(appendonly=true)"))
		})
		It("returns a slice for a column oriented table", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(rowCo...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetStorageOptions(connection, 0)
			Expect(results).To(Equal("(appendonly=true, orientation=column)"))
		})
		It("returns a slice for a heap table", func() {
			fakeResult := sqlmock.NewRows(header)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetStorageOptions(connection, 0)
			Expect(results).To(Equal(""))
		})
		It("returns a slice for an append-optimized table with a fill factor", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(rowAoFill...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetStorageOptions(connection, 0)
			Expect(results).To(Equal("(appendonly=true, fillfactor=42)"))
		})
		It("returns a slice for a column oriented table with a fill factor", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(rowCoFill...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetStorageOptions(connection, 0)
			Expect(results).To(Equal("(appendonly=true, orientation=column, fillfactor=42)"))
		})
		It("returns a slice for a heap table with a fill factor", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(rowFill...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetStorageOptions(connection, 0)
			Expect(results).To(Equal("(fillfactor=42)"))
		})
		It("returns a slice for a column oriented table with several storage options", func() {
			fakeResult := sqlmock.NewRows(header).AddRow(rowCoMany...)
			mock.ExpectQuery("SELECT (.*)").WillReturnRows(fakeResult)
			results := backup.GetStorageOptions(connection, 0)
			Expect(results).To(Equal("(appendonly=true, orientation=column, fillfactor=42, compresstype=zlib, blocksize=32768, compresslevel=1)"))
		})
	})
})
