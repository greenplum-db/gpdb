-- test gp_terminate_mpp_backends
1:create table gp_terminate_mpp_backends_t (a int);

select gp_terminate_mpp_backends() from gp_dist_random('gp_id');

-- expect following to fail as writer gang was killed
1:select count(*) from gp_terminate_mpp_backends_t;
