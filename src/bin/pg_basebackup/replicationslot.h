
#ifndef REPLICATIONSLOT_H
#define REPLICATIONSLOT_H

#include "postgres_fe.h"

typedef void (*create_physical_replication_slot_function)(const char* slot_name);


extern void CreateReplicationSlot(const char *replication_slot_name,
                                  create_physical_replication_slot_function create_replication_slot);
#endif