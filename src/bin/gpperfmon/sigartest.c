#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "apr.h"

#include "sigar.h"

void fatal_error(const char* msg)
{
    fprintf(stderr, "Fatal error: %s\n", msg);
    exit(1);
}

#define FMT64 APR_INT64_T_FMT

int main(int argc, char** argv)
{
    sigar_t* sigar = {0};

    sigar_file_system_list_t sigar_fslist = {0};
    sigar_net_interface_list_t sigar_netlist = {0};

    sigar_mem_t mem = {0};
    sigar_swap_t swap = {0};
    sigar_cpu_t cpu = {0};
    sigar_cpu_t pcpu = {0};
    int cpu_diff = 0;

    sigar_file_system_usage_t fsusage;
    sigar_disk_usage_t tdisk;
    sigar_disk_usage_t pdisk = {0};

    sigar_net_interface_stat_t net = {0};
    sigar_net_interface_stat_t tnet = {0};
    sigar_net_interface_stat_t pnet = {0};



    char** net_list = NULL;
    char** fs_list = NULL;
    char** fs = NULL;
    char** nic = NULL;

    int i;
    int clear = 1;
    int first = 1;
    int local_fs_count = 0;
    int local_fs_counter = 0;

    if (argc > 2 && strcmp(argv[1], "-c") == 0)
        clear = 1;

    if (sigar_open(&sigar) != 0)
    {
        fatal_error("Failed to open SIGAR");
    }
   
    printf("%d CPU cores detected\n", sigar_cpu_total_count(sigar));

    memset(&sigar_fslist, 0, sizeof(sigar_fslist));
    /* Get list of filesystems */
    if (sigar_file_system_list_get(sigar, &sigar_fslist) != 0)
    {
        fatal_error("Failed to get file system list");
    }

    for (i = 0; i < sigar_fslist.number; i++)
    {
        if (sigar_fslist.data[i].type == SIGAR_FSTYPE_LOCAL_DISK)
            local_fs_count++;
    }


    fs_list = (char **)malloc(sizeof(char *) * (local_fs_count + 1));
    memset(fs_list, 0, sizeof(char *) * (local_fs_count + 1));

    for (i = 0; i < sigar_fslist.number; i++)
    {
        if (sigar_fslist.data[i].type == SIGAR_FSTYPE_LOCAL_DISK)
        {
            fs_list[local_fs_counter++] = strdup(sigar_fslist.data[i].dev_name);
        }
    }
    sigar_file_system_list_destroy(sigar, &sigar_fslist);

    printf("Filesystems:\n");
    for (fs = fs_list; *fs; fs++)
    {
        printf("\t%s\n", *fs);
    }

    /* Get list of nets */
    memset(&sigar_netlist, 0, sizeof(sigar_netlist));
    if (sigar_net_interface_list_get(sigar, &sigar_netlist) != 0)
    {
        fatal_error("Failed to get net interface list");
    }

    net_list = (char **)malloc(sizeof(char *) * (sigar_netlist.number + 1));
    memset(net_list, 0, sizeof(char *) * (sigar_netlist.number + 1));

    for (i = 0; i < sigar_netlist.number; i++)
        net_list[i] = strdup(sigar_netlist.data[i]);

    printf("Net interfaces:\n");
    for (nic = net_list; *nic; nic++)
    {
        printf("\t%s\n", *nic);
    }

    sigar_net_interface_list_destroy(sigar, &sigar_netlist);

    sleep(5);
    system("clear");
    for (;;)
    {
        /* CPU */
        sigar_cpu_get(sigar, &cpu);
        cpu_diff = cpu.total - pcpu.total;
        
        printf("----------------------------------------------------------\n");
        printf("Devices\n");
        printf("----------------------------------------------------------\n");
        printf("CPU Diff: %d pcpu user: %" FMT64 "\n", cpu_diff, pcpu.user);
        printf("CPU:\tUser: %3.2f%%\tSystem: %3.2f%%\tIdle: %3.2f%%\n\t"
               "Wait: %3.2f%%\n", 
               (float)(cpu.user-pcpu.user) * 100 / cpu_diff, 
               (float)(cpu.sys-pcpu.sys) * 100 / cpu_diff, 
               (float)(cpu.idle-pcpu.idle) * 100 / cpu_diff, 
               (float)(cpu.wait-pcpu.wait) * 100 / cpu_diff);

        pcpu = cpu;

        /* Memory */
        sigar_mem_get(sigar, &mem);
        printf("Memory:\tTotal: %" FMT64 "\tUsed: %" FMT64 "\tFree: %" FMT64 "\n", 
               mem.total, mem.used, mem.free);

        /* Swap */
        sigar_swap_get(sigar, &swap);
        printf("Swap:\tTotal: %" FMT64 "\tUsed: %" FMT64 "\tFree: %" FMT64 "\n", 
               swap.total, swap.used, swap.free);

        /* Network */
        memset(&tnet, 0, sizeof(tnet));
        for (nic = net_list; *nic; nic++)
        {
            int e = sigar_net_interface_stat_get(sigar, *nic, &net);
            if (e != 0)
            {
                printf("Error retrieving net stats for %s\n", *nic);
                continue;
            }
            printf("Stats for %s:\n", *nic);
            printf("\tBytes Sent: %" FMT64 "\tBytes Recv: %" FMT64 "\n", 
                   net.tx_bytes, net.rx_bytes);
            printf("\tPackets Sent %" FMT64 "\tPackets Revc: %" FMT64 "\n", 
                   net.tx_packets, net.rx_packets);
            tnet.tx_bytes += net.tx_bytes;
            tnet.rx_bytes += net.rx_bytes;
            tnet.tx_packets += net.tx_packets;
            tnet.rx_packets += net.rx_packets;
        }

        /* Disk */
        memset(&tdisk, 0, sizeof(tdisk));
        for (fs = fs_list; *fs; fs++)
        {
            int e = sigar_file_system_usage_get(sigar, *fs, &fsusage);
            if (e != 0)
            {
                printf("Error retrieving disk stats for %s\n", *fs);
                continue;
            }
            printf("Stats for %s:\n", *fs);
            printf("\tReads: %" FMT64 "\tWrites: %" FMT64 "\n", 
                   fsusage.disk.reads, fsusage.disk.writes);
            printf("\tBytes Read: %" FMT64 "\tBytes Written: %" FMT64 "\n", 
                   fsusage.disk.read_bytes, fsusage.disk.write_bytes);
            tdisk.reads += fsusage.disk.reads;
            tdisk.writes += fsusage.disk.writes;
            tdisk.write_bytes += fsusage.disk.write_bytes;
            tdisk.read_bytes += fsusage.disk.read_bytes;
            
        }

        if (first)
        {
            pnet = tnet; pdisk = tdisk; 
        }

        first = 0;

        printf("----------------------------------------------------------\n");
        printf("Per second\n");
        printf("----------------------------------------------------------\n");
        printf("Bytes Rx/sec: %" FMT64 "\t", tnet.rx_bytes - pnet.rx_bytes);
        printf("Bytes Tx/sec: %" FMT64 "\n", tnet.tx_bytes - pnet.tx_bytes);
        printf("Packets Rx/sec: %" FMT64 "\t", 
               tnet.rx_packets - pnet.rx_packets);
        printf("Packets Tx/sec: %" FMT64 "\n", 
               tnet.tx_packets - pnet.tx_packets);
        printf("Reads/sec: %" FMT64 "\t", tdisk.reads - pdisk.reads);
        printf("Writes/sec: %" FMT64 "\n", tdisk.writes - pdisk.writes);
        printf("Bytes Read/sec: %" FMT64 "\t", 
               tdisk.read_bytes - pdisk.read_bytes);
        printf("Bytes Written/sec: %" FMT64 "\n", 
               tdisk.write_bytes - pdisk.write_bytes);
        printf("----------------------------------------------------------\n");
        
        pnet = tnet; pdisk = tdisk; 


        sleep(1);
        if (clear)
            system("clear");
    }


    if (sigar_close(sigar) != 0)
    {
        fatal_error("Failed to close SIGAR");
    }


    return 0;
}
