UPDATE gpexpand.status_detail SET rank= 10; 

UPDATE gpexpand.status_detail SET rank=1 WHERE fq_name = 'public.users_rank_1';
UPDATE gpexpand.status_detail SET rank=2 WHERE fq_name = 'public.users_rank_2';
UPDATE gpexpand.status_detail SET rank=3 WHERE fq_name = 'public.users_rank_3';
