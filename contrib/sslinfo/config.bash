# This bash script is internally used by sslinfo test
function sslinfo_prepare() {
echo "Enable SSL in postgresql.conf with master only..."
standby_data=`gpstate -f | sed -n '/Standby data directory/s/.*Standby data directory\s\+=\s*//p'`

echo "#BEGIN SSLINFO CONF : BEGIN ANCHOR##" >> $MASTER_DATA_DIRECTORY/postgresql.conf
echo "ssl=on" >> $MASTER_DATA_DIRECTORY/postgresql.conf
echo "ssl_ciphers='HIGH:MEDIUM:+3DES:!aNULL'" >> $MASTER_DATA_DIRECTORY/postgresql.conf
echo "ssl_cert_file='server.crt'" >> $MASTER_DATA_DIRECTORY/postgresql.conf
echo "ssl_key_file='server.key'" >> $MASTER_DATA_DIRECTORY/postgresql.conf
echo "ssl_ca_file='root.crt'" >> $MASTER_DATA_DIRECTORY/postgresql.conf
echo "#END SSLINFO CONF : END ANCHOR##" >> $MASTER_DATA_DIRECTORY/postgresql.conf

echo "#BEGIN SSLINFO CONF : BEGIN ANCHOR##" >> $standby_data/postgresql.conf
echo "ssl=on" >> $standby_data/postgresql.conf
echo "ssl_ciphers='HIGH:MEDIUM:+3DES:!aNULL'" >> $standby_data/postgresql.conf
echo "ssl_cert_file='server.crt'" >> $standby_data/postgresql.conf
echo "ssl_key_file='server.key'" >> $standby_data/postgresql.conf
echo "ssl_ca_file='root.crt'" >> $standby_data/postgresql.conf
echo "#END SSLINFO CONF : END ANCHOR##" >> $standby_data/postgresql.conf

echo "preparing CRTs and KEYs"
cp -f data/root.crt   $MASTER_DATA_DIRECTORY/
cp -f data/server.crt $MASTER_DATA_DIRECTORY/
cp -f data/server.key $MASTER_DATA_DIRECTORY/
chmod 400 $MASTER_DATA_DIRECTORY/server.key
chmod 644 $MASTER_DATA_DIRECTORY/server.crt
chmod 644 $MASTER_DATA_DIRECTORY/root.crt

cp -f data/root.crt   $standby_data/
cp -f data/server.crt $standby_data/
cp -f data/server.key $standby_data/
chmod 400 $standby_data/server.key
chmod 644 $standby_data/server.crt
chmod 644 $standby_data/root.crt

mkdir -p ~/.postgresql
cp -f data/root.crt         ~/.postgresql/
cp -f data/postgresql.crt   ~/.postgresql/
cp -f data/postgresql.key   ~/.postgresql/
chmod 400 ~/.postgresql/postgresql.key
chmod 644 ~/.postgresql/postgresql.crt
chmod 644 ~/.postgresql/root.crt
}

function sslinfo_clean() {
echo "restore SSL in postgresql.conf with master only"
standby_data=`gpstate -f | sed -n '/Standby data directory/s/.*Standby data directory\s\+=\s*//p'`
sed -i '/#BEGIN SSLINFO CONF : BEGIN ANCHOR##/,/#END SSLINFO CONF : END ANCHOR##/d' $MASTER_DATA_DIRECTORY/postgresql.conf
sed -i '/#BEGIN SSLINFO CONF : BEGIN ANCHOR##/,/#END SSLINFO CONF : END ANCHOR##/d' $standby_data/postgresql.conf
}

case "$1" in
prepare)
    sslinfo_prepare
    ;;
clean)
    sslinfo_clean
    ;;
*)
    echo "$0 { prepare | clean }"
    exit 1
esac
