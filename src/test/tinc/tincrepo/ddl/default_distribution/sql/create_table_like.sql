select attrnums from gp_distribution_policy where
  localoid = 'person'::regclass;

-- test that LIKE clause does not affect default distribution
select attrnums from gp_distribution_policy where
  localoid = 'person_copy'::regclass;
