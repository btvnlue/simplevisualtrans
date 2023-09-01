#pragma once

class BufferedJsonAllocator
{
	long maxtotal;
	long totalsize;
	long currpos;
	long currindex;
	long currsize;
	unsigned char** buffer;
	//long index;

	int _alloc_curr_index(long cidx, long csz);
	static BufferedJsonAllocator* self;
	void* _Malloc(size_t size);
	void* _Realloc(void* originalPtr, size_t originalSize, size_t newSize);
	int _Reset();
public:
	BufferedJsonAllocator();
	virtual ~BufferedJsonAllocator();
	static const bool kNeedFree;    //!< Whether this allocator needs to call Free().
	void* Malloc(size_t size);
	void* Realloc(void* originalPtr, size_t originalSize, size_t newSize);

	static void Free(void *ptr);
	static BufferedJsonAllocator* GetInstance();
	int Reset();
	int FreeAll();
};