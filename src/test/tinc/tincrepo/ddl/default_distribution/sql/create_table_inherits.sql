select attrnums from gp_distribution_policy where
  localoid = 'staff_member'::regclass;

select attrnums from gp_distribution_policy where
  localoid = 'student'::regclass;

select attrnums from gp_distribution_policy where
  localoid = 'stud_emp'::regclass;
