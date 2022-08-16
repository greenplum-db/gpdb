-- We should disallow COPY FROM and INSERT in utility mode if the target table
-- is a root of a partition/inheritance hierarchy.

CREATE TABLE utility_part_root (a int, b int, c int)
    DISTRIBUTED BY (a)
    PARTITION BY range(b)
        SUBPARTITION BY range(c)
            SUBPARTITION TEMPLATE (
            START(40) END(46) EVERY(3)
            )
        (START(0) END(4) EVERY(2));

-- should complain as we are trying copy on a root or subroot
1U: COPY utility_part_root FROM '/dev/null';
1U: COPY utility_part_root_1_prt_1 FROM '/dev/null';

-- should be fine as we are trying it on a leaf
1U: COPY utility_part_root_1_prt_1_2_prt_1 FROM '/dev/null';

-- should complain as we are trying insert on a root or subroot
1U: INSERT INTO utility_part_root VALUES(1, 2, 41);
1U: INSERT INTO utility_part_root_1_prt_1 VALUES(1,0,40);

-- should be fine as we are trying it on a leaf
1U: INSERT INTO utility_part_root_1_prt_1_2_prt_1 VALUES(1,0,40);

CREATE TABLE utility_inh_root (
    a int
);
CREATE TABLE utility_inh_subroot (
    b int
) INHERITS (utility_inh_root);
CREATE TABLE utility_inh_leaf (
    c int
) INHERITS (utility_inh_subroot);

-- should complain as we are trying copy on a root or subroot
1U: COPY utility_inh_root FROM '/dev/null';
1U: COPY utility_inh_subroot FROM '/dev/null';

-- should be fine as we are trying it on a leaf
1U: COPY utility_inh_leaf FROM '/dev/null';

-- should complain as we are trying insert on a root or subroot
1U: INSERT INTO utility_inh_root VALUES(1);
1U: INSERT INTO utility_inh_subroot VALUES(1, 2);

-- should be fine as we are trying it on a leaf
1U: INSERT INTO utility_inh_leaf VALUES(1, 2, 3);
