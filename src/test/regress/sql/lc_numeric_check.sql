---
--- Set idle gang time to 1ms (minimum)
--- Want to verify if new segment process get lc_numeric values always
---
set gp_vmem_idle_resource_timeout = 1;


---
--- Drop existing table
---
DROP TABLE IF EXISTS lc_numeric_table;

---
--- Create new table lc_numeric_table
---
CREATE TABLE lc_numeric_table(a text, b varchar(50));

---
--- Insert some values
---
INSERT INTO lc_numeric_table VALUES
  ('1,1', 'bla'),
  (2, 'blabla'),
  (3, 'blablabla');

---
--- Select query with default lc_numeric
--- expect first col value for '1,1' is '11'
---
select to_number(a,'999999D99999')::numeric(10,5), b from lc_numeric_table order by to_number;

---
--- Change lc_numeric
---
set lc_numeric='de_DE';

---
--- Select query with default lc_numeric
--- expect first col value for '1,1' is '1.1'
---
select to_number(a,'999999D99999')::numeric(10,5), b from lc_numeric_table order by to_number;

-- start_ignore
\echo `date`
-- end_ignore

-- start_ignore
\echo `date`
-- end_ignore

---
--- Select query with default lc_numeric
--- expect first col value for '1,1' is '1.1'
---
select to_number(a,'999999D99999')::numeric(10,5), b from lc_numeric_table order by to_number;


---
--- reset all gucs
---
reset lc_numeric;
reset gp_vmem_idle_resource_timeout;
