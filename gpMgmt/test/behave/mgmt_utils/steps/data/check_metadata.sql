-- Check table indexes, modifiers, and options
\d+ aoco_table
\d+ heap_table

-- Check user-defined functions and aggregate functions
\df

-- Check triggers
select tgname, tgtype from pg_trigger where tgname='before_heap_ins_trig';

-- Check user-defined types
\dT

-- Check user-defined operators
\do

-- Check user-defined conversions
\dc
