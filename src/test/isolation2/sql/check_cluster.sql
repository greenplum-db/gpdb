select dbid, content, status, role, preferred_role, mode from gp_segment_configuration where status = 'd' or preferred_role != role;
