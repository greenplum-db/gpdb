#! /bin/bash
## Takes args $1 (NUM_COPIES) and $2 (GPFDIST_PORT)

# Create dataset directory that gpfdist will host
if [ -d $MASTER_DATA_DIRECTORY/perfdataset ]; then
  rm -rf $MASTER_DATA_DIRECTORY/perfdataset
fi
mkdir $MASTER_DATA_DIRECTORY/perfdataset

# Generate dataset (needs refactoring to make it better)
for i in $(seq ${1}); do
  cat dataset/perfdata.csv >> $MASTER_DATA_DIRECTORY/perfdataset/perfdata.csv;
done

# Kill gpfdist processes and host the dataset
killall gpfdist
sleep 5
gpfdist -p $2 -d $MASTER_DATA_DIRECTORY/perfdataset -l $MASTER_DATA_DIRECTORY/perfdataset/gpfdist.log &

# Update sql and ans file with hostname and gpfdist port
cat ./sql/setup.sql.template | sed "s/@hostname@:@gpfdist_port@/${HOSTNAME}:${2}/" > ./sql/setup.sql
cat ./expected/setup.out.template | sed "s/@hostname@:@gpfdist_port@/${HOSTNAME}:${2}/" > ./expected/setup.out
