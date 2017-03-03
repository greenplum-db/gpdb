#ifndef __S3MEMORY_MGMT_H__
#define __S3MEMORY_MGMT_H__

#include "s3common_headers.h"
#include "s3exception.h"
#include "s3macros.h"

void* S3Alloc(size_t);
void S3Free(void*);

class PreAllocatedMemory {
   public:
    PreAllocatedMemory(size_t chunkSize, size_t numOfChunk) {
        maxSize = chunkSize * numOfChunk;
        // we will have no more than 9 chunks, 8 for thread thunk, one for main buffer.
        // Each chunk is limited to 128MB.
        const uint64_t memoryLimit = 9 * 128 * 1024 * 1024;
        S3_CHECK_OR_DIE(maxSize <= memoryLimit, S3MemoryOverLimit, memoryLimit, maxSize);

        used.resize(numOfChunk);
        chunks.resize(numOfChunk);
        for (size_t i = 0; i < numOfChunk; i++) {
            chunks[i] = S3Alloc(chunkSize);
            if (chunks[i] == NULL) {
                for (size_t j = 0; j < i; j++) {
                    S3Free(chunks[j]);
                }
                S3_DIE(S3AllocationError, chunkSize);
            }
            used[i] = false;
        }

        pthread_mutex_init(&memLock, NULL);
    }

    ~PreAllocatedMemory() {
        for (size_t i = 0; i < chunks.size(); i++) {
            if (chunks[i]) {
                S3Free(chunks[i]);
                chunks[i] = NULL;
            }
        }

        pthread_mutex_destroy(&memLock);
    }

    size_t MaxSize() const {
        return maxSize;
    }

    void* Allocate() {
        UniqueLock lock(&memLock);
        for (size_t i = 0; i < used.size(); i++) {
            if (!used[i]) {
                used[i] = true;
                return chunks[i];
            }
        }

        S3_DIE(S3RuntimeError, "Requested more than preallocated memory");
    }

    void Deallocate(void* p) {
        UniqueLock lock(&memLock);
        for (size_t i = 0; i < used.size(); i++) {
            if (chunks[i] == p) {
                used[i] = false;
                return;
            }
        }
        stringstream ss;
        ss << "Free invalid memory: " << p;
        S3_DIE(S3RuntimeError, ss.str());
    }

   private:
    PreAllocatedMemory(const PreAllocatedMemory&);
    PreAllocatedMemory& operator=(const PreAllocatedMemory&);

    size_t maxSize;
    vector<bool> used;
    vector<void*> chunks;
    pthread_mutex_t memLock;
};

template <class T>
struct PGAllocator {
    PGAllocator() {
    }

    template <class U>
    PGAllocator(const PGAllocator<U>& other) {
        prealloc = other.prealloc;
    }

    typedef size_t size_type;
    typedef ptrdiff_t difference_type;
    typedef T* pointer;
    typedef const T* const_pointer;
    typedef T& reference;
    typedef const T& const_reference;
    typedef T value_type;

    size_type max_size() const {
        if (prealloc) {
            return prealloc->MaxSize();
        } else {
            return std::allocator<T>().max_size();
        }
    }

    template <class U>
    struct rebind {
        typedef PGAllocator<U> other;
    };

    T* allocate(size_t n) {
        if (prealloc) {
            return (T*)prealloc->Allocate();
        } else {
            return std::allocator<T>().allocate(n);
        }
    }

    T* allocate(std::size_t n, const void*) {
        return allocate(n);
    }

    void construct(T* p, const T& val) {
        new (p) T(val);
    }

    void destroy(T* p) {
        p->~T();
    }

    void deallocate(T* p, std::size_t n) {
        if (prealloc) {
            prealloc->Deallocate(p);
        } else {
            std::allocator<T>().deallocate(p, n);
        }
    }

    void prepare(size_t chunkSize, size_t numOfChunk) {
        prealloc.reset();
        prealloc.reset(new PreAllocatedMemory(chunkSize, numOfChunk));
    }

    std::shared_ptr<PreAllocatedMemory> prealloc;
};

template <class T, class U>
bool operator==(const PGAllocator<T>& a, const PGAllocator<U>& b) {
    return a.prealloc == b.prealloc;
}

template <class T, class U>
bool operator!=(const PGAllocator<T>& a, const PGAllocator<U>& b) {
    return !(a == b);
}

typedef PGAllocator<uint8_t> S3MemoryContext;

class S3VectorUInt8 : public std::vector<uint8_t, S3MemoryContext> {
   public:
    explicit S3VectorUInt8() : std::vector<uint8_t, S3MemoryContext>(S3MemoryContext()) {
    }

    explicit S3VectorUInt8(size_t size)
        : std::vector<uint8_t, S3MemoryContext>(size, 0, S3MemoryContext()) {
    }

    explicit S3VectorUInt8(const std::vector<uint8_t>& v)
        : std::vector<uint8_t, S3MemoryContext>(v.begin(), v.end(), S3MemoryContext()) {
    }

    explicit S3VectorUInt8(const S3MemoryContext& context)
        : std::vector<uint8_t, S3MemoryContext>(context) {
    }

    void release() {
        // clear() will not release the memory, we use the swap approach to force release memory.
        S3VectorUInt8(this->get_allocator()).swap(*this);
    }
};

#endif
