-- start_ignore
drop table emp cascade;
-- end_ignore
CREATE TABLE emp (name text, age int, salary numeric(10,2));
INSERT INTO emp VALUES ('Joe', 41, 250000.00);
INSERT INTO emp VALUES ('Jim', 25, 120000.00);
INSERT INTO emp VALUES ('Jon', 35, 50000.00);
CREATE OR REPLACE FUNCTION overpaid (emp) RETURNS bool AS '
    if (200000 < arg1$salary) {
        return(TRUE)
    }
    if (arg1$age < 30 && 100000 < arg1$salary) {
        return(TRUE)
    }
    return(FALSE)
' LANGUAGE 'plr';
SELECT name, overpaid(emp) FROM emp;

CREATE OR REPLACE FUNCTION get_emps() RETURNS SETOF emp AS '
    names <- c("Joe","Jim","Jon")
    ages <- c(41,25,35)
    salaries <- c(250000,120000,50000)
    df <- data.frame(name = names, age = ages, salary = salaries)
    return(df)
' LANGUAGE 'plr';
select * from get_emps();
