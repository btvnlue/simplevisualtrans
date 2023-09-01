#include "Utilities.h"

#include <malloc.h>
#include <memory.h>

const bool BufferedJsonAllocator::kNeedFree = false;
BufferedJsonAllocator* BufferedJsonAllocator::self = nullptr;

int BufferedJsonAllocator::_alloc_curr_index(long cidx, long csz)
{
	int rtn;
	buffer[cidx] = (unsigned char*)malloc(csz);
	totalsize += currsize;
	currpos = 0;

	rtn = buffer[cidx] ? csz : 0;
	return rtn;
}

int BufferedJsonAllocator::FreeAll()
{
	if (currindex >= 0) {
		for (int ii = 0; ii < currindex; ii++) {
			free(buffer[ii]);
		}
		free(buffer);
		currindex = -1;
	}
	return 0;
}


BufferedJsonAllocator::BufferedJsonAllocator()
	: totalsize(0)
	//, index(0)
	, currpos(0)
	, maxtotal(0)
{
}

BufferedJsonAllocator::~BufferedJsonAllocator()
{
	//FreeAll();
	//free(buffer);
	//buffer = NULL;
}

void * BufferedJsonAllocator::Malloc(size_t size)
{
	void * rtn = nullptr;
	BufferedJsonAllocator* me = GetInstance();
	if (me) {
		rtn = me->_Malloc(size);
	}
	return rtn;
}

void * BufferedJsonAllocator::Realloc(void * originalPtr, size_t originalSize, size_t newSize)
{
	void * rtn = nullptr;
	BufferedJsonAllocator* me = GetInstance();
	if (me) {
		rtn = me->_Realloc(originalPtr, originalSize, newSize);
	}
	return rtn;
}

#ifndef __max
#define __max(a,b) (((a) > (b)) ? (a) : (b))
#endif

void * BufferedJsonAllocator::_Malloc(size_t size)
{
	unsigned char * rtn = nullptr;

	if (currpos + (int)size > currsize) {
		currindex++;
		currsize = __max(currsize * 2, (int)size);
		_alloc_curr_index(currindex, currsize);
		currpos = 0;
	}
	rtn = buffer[currindex] + currpos;
	currpos += (int)size;
	if (currpos >= currsize) {
		currindex++;
		currsize *= 2;
		_alloc_curr_index(currindex, currsize);
		currpos = 0;
	}

	return rtn;
}

void * BufferedJsonAllocator::_Realloc(void * originalPtr, size_t originalSize, size_t newSize)
{
	void* rtn = _Malloc(newSize);
	if (rtn) {
		memcpy_s(rtn, newSize, originalPtr, originalSize);
	}
	return rtn;
}

int BufferedJsonAllocator::_Reset()
{
	if (totalsize > maxtotal) {
		maxtotal = totalsize + (((unsigned long)totalsize) >> 3);

		FreeAll();
		buffer = (unsigned char**)malloc(sizeof(unsigned char*) * 1024);
		currsize = maxtotal;
		currindex = 0;
		totalsize = 0;
		currpos = 0;
		_alloc_curr_index(self->currindex, self->currsize);
	}
	else {
		currindex = 0;
		currpos = 0;
	}
	return 0;
}

void BufferedJsonAllocator::Free(void * ptr)
{
}

BufferedJsonAllocator * BufferedJsonAllocator::GetInstance()
{
	if (self == nullptr) {
		self = new BufferedJsonAllocator();
		self->buffer = (unsigned char**)malloc(sizeof(unsigned char*) * 1024);
		self->currsize = 4096;
		self->currindex = 0;
		self->_alloc_curr_index(self->currindex, self->currsize);
	}
	return self;
}

int BufferedJsonAllocator::Reset()
{
	int rtn = 0;
	BufferedJsonAllocator* me = GetInstance();
	if (me) {
		rtn = me->_Reset();
	}
	return rtn;

}
