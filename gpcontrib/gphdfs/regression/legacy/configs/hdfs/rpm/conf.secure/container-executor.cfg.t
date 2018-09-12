yarn.nodemanager.local-dirs=YARN_TMP_DIR/${user.name}/nm-local-dir
yarn.nodemanager.log-dirs=YARN_LOG_DIR/containers
# configured value of yarn.nodemanager.linux-container-executor.group
yarn.nodemanager.linux-container-executor.group=yarn
# comma separated list of users who can not run applications
banned.users=hdfs,yarn,mapred,bin
# Prevent other super-users
min.user.id=500