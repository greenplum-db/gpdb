-- PRIMARY KEY default behavior
select attrnums from gp_distribution_policy where
  localoid = 'distpol'::regclass;

alter table dist add primary key (j);
