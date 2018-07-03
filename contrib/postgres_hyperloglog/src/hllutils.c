/* Contains utility functions used for hyperloglog. Such as the hash function,
 * the dedupe function, and the sparse_array_size function. */
#include <stdlib.h>
#include <math.h>

#include "postgres.h"
#include "hyperloglog.h"
#include "hllutils.h"

/* ---------------------- function definitions --------------------------- */

/* MurmurHash64A produces the fastest 64 bit hash of the MurmurHash 
 * implementations and is ~ 20x faster than md5. This version produces the
 * same hash for the same key and seed in both big and little endian systems
 * */
uint64_t 
MurmurHash64A (const void * key, int len, unsigned int seed) 
{
    const uint64_t m = 0xc6a4a7935bd1e995;
    const int r = 47;
    uint64_t h = seed ^ (len * m);
    const uint8_t *data = (const uint8_t *)key;
    const uint8_t *end = data + (len-(len&7));

    while(data != end) {
        uint64_t k;

#if (BYTE_ORDER == LITTLE_ENDIAN)
        k = *((uint64_t*)data);
#else
        k = (uint64_t) data[0];
        k |= (uint64_t) data[1] << 8;
        k |= (uint64_t) data[2] << 16;
        k |= (uint64_t) data[3] << 24;
        k |= (uint64_t) data[4] << 32;
        k |= (uint64_t) data[5] << 40;
        k |= (uint64_t) data[6] << 48;
        k |= (uint64_t) data[7] << 56;
#endif

        k *= m;
        k ^= k >> r;
        k *= m;
        h ^= k;
        h *= m;
        data += 8;
    }

    switch(len & 7) {
        case 7: h ^= (uint64_t)data[6] << 48;
        case 6: h ^= (uint64_t)data[5] << 40;
        case 5: h ^= (uint64_t)data[4] << 32;
        case 4: h ^= (uint64_t)data[3] << 24;
        case 3: h ^= (uint64_t)data[2] << 16;
        case 2: h ^= (uint64_t)data[1] << 8;
        case 1: h ^= (uint64_t)data[0];
        h *= m;
    };

    h ^= h >> r;
    h *= m;
    h ^= h >> r;

    return h;
}

void
insertion_sort(uint32_t *a, int n) 
{
    size_t i, j;
    uint32_t value;
    for (i = 1; i < n; i++) {
        value = a[i];
        for (j = i; j > 0 && value < a[j - 1]; j--) {
            a[j] = a[j - 1];
        }
        a[j] = value;
    }
}

/* First sorts the sparse data and then removes duplicates and returns the new
 * index value (i.e. the number of entries). */
int 
dedupe(uint32_t * sparse_data, int idx)
{
    int i,j;

    for ( i=0; i < idx - 1; i++){
        if (sparse_data[i] > sparse_data[i+1]){
            insertion_sort(sparse_data,idx);
            break;
        }
    }

    j = 0;
    for (i=0; i < idx; i++){
        if (i == 0){
            j++;
            continue;
        } else if (sparse_data[i] != sparse_data[j - 1]){
            sparse_data[j++] = sparse_data[i];
        }
    }

    memset(&sparse_data[j],0,idx - j);

    return j;
}

/* Returns the maximum number of 32 bits ints that can be stored in the sparse
 * array */
int 
size_sparse_array(int8_t b)
{
    return  POW2(b-4) - ceil(sizeof(HLLData)/4.0);
}
