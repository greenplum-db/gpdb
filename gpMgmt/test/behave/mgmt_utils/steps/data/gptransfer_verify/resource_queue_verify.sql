SELECT u.rolname, u.rolsuper, r.rsqname FROM pg_roles as u, pg_resqueue as r WHERE u.rolresqueue=r.oid and rolname='reg_u1';
SELECT u.rolname, u.rolsuper, r.rsqname FROM pg_roles as u, pg_resqueue as r WHERE u.rolresqueue=r.oid and r.rsqname='reg_activeq';
