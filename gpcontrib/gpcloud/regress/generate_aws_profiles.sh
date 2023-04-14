#!/bin/bash
AWS_PROFILES_DIR=$HOME/.aws
AWS_PROFILES=$AWS_PROFILES_DIR/credentials

if [ ! -e "$AWS_PROFILES_DIR" ]; then
    mkdir -p ${AWS_PROFILES_DIR}
fi

if [ ! -d "$AWS_PROFILES_DIR" ]; then
    echo "ERROR:  $AWS_PROFILES_DIR is not directory."
    exit 1
fi

if [ -f "$AWS_PROFILES" ]; then
    echo "Warning: file $AWS_PROFILES already exists."
    exit 0
fi

if [ -n "$gpcloud_access_key_id" ] && [ -n "$gpcloud_secret_access_key" ]; then
    cat > $AWS_PROFILES <<-EOF
    [no_accessid_secret]
    aws_access_key_id = "$gpcloud_access_key_id"
    aws_secret_access_key = "$gpcloud_secret_access_key"
EOF
else
    echo "Error: environment varibles \$gpcloud_access_key_id and \$gpcloud_secret_access_key are not set."
    exit 1
fi

exit 0
