#include "replicationslot.h"

void CreateReplicationSlot(const char *replication_slot_name,
                           create_physical_replication_slot_function
                           create_replication_slot) {
	create_replication_slot(replication_slot_name);
}