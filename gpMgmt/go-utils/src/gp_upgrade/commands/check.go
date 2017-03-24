package commands

import "os"

//select hostname, content, dbid, port, fselocation as datadir from gp_segment_configuration, pg_filespace_entry where fsedbid = dbid;
//import (
//"database/sql"
//"encoding/json"
//"fmt"

//"os"

//_ "github.com/lib/pq"
//)

type CheckCommand struct {
	Master_host string `long:"master_host" required:"yes" description:"Domain name or IP of host"`
	Master_port int    `long:"master_port" required:"no" default:"5432" description:"Port for master database"`
}

const (
	host   = "localhost"
	port   = 15432
	user   = "pivotal"
	dbname = "template1"
)

func (cmd CheckCommand) Execute([]string) error {
	//psqlInfo := fmt.Sprintf("host=%s port=%d user=%s "+
	//	"dbname=%s sslmode=disable",
	//	host, port, user, dbname)
	//db, err := sql.Open("postgres", psqlInfo)
	//if err != nil {
	//	return (err)
	//}
	//defer db.Close()
	//
	//err = db.Ping()
	//if err != nil {
	//	return (err)
	//}
	//
	//rows, err := db.Query(`SELECT content, dbid FROM gp_segment_configuration`)
	//if err != nil {
	//	return (err)
	//}
	//defer rows.Close()
	//columns, err := rows.Columns()
	//if err != nil {
	//	return (err)
	//}
	//count := len(columns)
	//tableData := make([]map[string]interface{}, 0)
	//values := make([]interface{}, count)
	//valuePtrs := make([]interface{}, count)
	//for rows.Next() {
	//	for i := 0; i < count; i++ {
	//		valuePtrs[i] = &values[i]
	//	}
	//	rows.Scan(valuePtrs...)
	//	entry := make(map[string]interface{})
	//	for i, col := range columns {
	//		var v interface{}
	//		val := values[i]
	//		b, ok := val.([]byte)
	//		if ok {
	//			v = string(b)
	//		} else {
	//			v = val
	//		}
	//		entry[col] = v
	//	}
	//	tableData = append(tableData, entry)
	//}
	//jsonData, err := json.Marshal(tableData)
	//if err != nil {
	//	return (err)
	//}
	//fmt.Println(string(jsonData))
	//
	path := os.Getenv("HOME") + "/.gp_upgrade"
	os.MkdirAll(path, 0700)
	f, _ := os.Create(path + "/cluster_config.json")
	defer f.Close()
	//if err != nil {
	//	log.Fatal(err)
	//}
	//defer f.Close()
	//_, err = f.WriteString("test")
	//_, err = f.WriteString(string(jsonData))
	//if err != nil {
	//return err
	//}
	//
	//fmt.Println("Successfully connected!")
	return nil
}
