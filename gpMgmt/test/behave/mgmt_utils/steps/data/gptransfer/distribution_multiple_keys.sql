create table table_distribution("id1,"" " int, "id2 crazy""'" int, name text) distributed by ( "id1,"" ", "id2 crazy""'");
create table reverse_table_distribution("id1,"" " int, "id2 crazy""'" int, name text) distributed by ("id2 crazy""'", "id1,"" ");
