gp_dump_agent --gp-k 11111111111111_1_1_ --gp-d /tmp --pre-data-schema-only nbutestdb --table-file=/tmp/dirty_hack.txt --netbackup-service-host=cdw --netbackup-policy=test_backup --netbackup-schedule=app_backup_schedule | gp_bsa_dump_agent --netbackup-service-host=cdw --netbackup-policy=test_backup --netbackup-schedule=app_backup_schedule --netbackup-filename
