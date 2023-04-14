# pg_attrdef 

The `pg_attrdef` table stores column default values. The main information about columns is stored in [pg\_attribute](pg_attribute.html). Only columns that explicitly specify a default value \(when the table is created or the column is added\) will have an entry here.

|column|type|references|description|
|------|----|----------|-----------|
|`adrelid`|oid|pg\_class.oid|The table this column belongs to|
|`adnum`|int2|pg\_attribute.attnum|The number of the column|
|`adbin`|text| |The internal representation of the column default value|
|`adsrc`|text| |A human-readable representation of the default value. This field is historical, and is best not used.|

**Parent topic:** [System Catalogs Definitions](../system_catalogs/catalog_ref-html.html)

