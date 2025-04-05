#include "../localTypes.h"

typedef struct Chunk {
	struct Chunk	*next;
	u32				size;
	u32				data[1];
} Chunk;

typedef struct {
	u8          *heapCursor;
	Chunk       *chunks;
} Heap;

static Heap h = {
	(u8*)HEAP_TOP_ADDR,
	0
};

static void *zalloc_internal(u32 nByte)
{
	asm("CPSID i"); // disable interrupts
	Chunk **cursor = &h.chunks;
	Chunk *target = 0;
	while (*cursor)
	{
		if ((*cursor)->size == nByte)
		{
			target = *cursor;
			*cursor = (*cursor)->next;
			break;
		}
		cursor = &(*cursor)->next;
	}
	if (target == 0)
	{
		h.heapCursor -= nByte;
		target = (void*)h.heapCursor;
	}
	asm("CPSIE i"); // enable interrupts

	target->size = nByte;
	u32 *mem = target->data;
	u32 *end = (u32*)((u32)mem + nByte - 8);
	do {
		mem[0] = 0;
		mem[1] = 0;
		mem += 2;
	} while (mem < end);
	return target->data;
}


/*
** Free an outstanding memory allocation.
*/
static void free_internal(void *pOld)
{
	u8 *raw = pOld;
	Chunk *target = (void*)(raw-8);
	target->next = h.chunks;
	h.chunks = target;
}

/*e*/
void free(void *pOld)/*p;*/
{
	if (pOld==0) { return; }
	asm("CPSID i");  // disable interrupts
	//~ os_takeSpinLock(LOCK_MEMORY_ALLOC);
	free_internal(pOld);
	//~ os_giveSpinLock(LOCK_MEMORY_ALLOC);
	asm("CPSIE i"); // enable interrupts
}

/*e*/
void *zalloc(u32 nByte)/*p;*/
{
	// if nByte is 0 -> return 0 to be consistent with realloc
	if (nByte==0) { return 0; }
	//~ asm("CPSID i"); // disable interrupts
	//~ os_takeSpinLock(LOCK_MEMORY_ALLOC);
	void *memory = zalloc_internal((nByte + 15) & -8);
	//~ os_giveSpinLock(LOCK_MEMORY_ALLOC);
	//~ asm("CPSIE i"); // enable interrupts
	return memory;
}

