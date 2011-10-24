#include "mem.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdint.h>


struct mem_node* consolidateBefore(struct mem_node *newNode);
struct mem_node* consolidateAfter(struct mem_node *newNode);

struct mem_node {
	struct mem_node *next;
	struct mem_node *prev;
	int	  size;
};

struct mem_node *freeHead = NULL;
int freeSize = 0;
int m_error;

int main()
{
	return 0;
}

int Mem_Init(int sizeOfRegion, int debug)
{
	if(sizeOfRegion > 0 && freeHead == NULL )
	{
		int pageSize = getpagesize();
		if(sizeOfRegion % getpagesize() != 0)
			sizeOfRegion += getpagesize() - (sizeOfRegion % getpagesize());
		

		int fd = open("/dev/zero", O_RDWR);

		void* ptr = mmap(NULL, sizeOfRegion, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
		if (ptr == MAP_FAILED) { perror("mmap"); exit(1); }
		close(fd);

		//get 8 byte alligned
		if((uintptr_t)ptr  % 8 != 0)
		{
			ptr += 8 - ((uintptr_t)ptr % 8);
			sizeOfRegion -= 8 - ((uintptr_t)ptr % 8);
		}
		freeHead = ptr;
		freeHead->next = NULL;
		freeHead->prev = NULL;
		freeHead->size = sizeOfRegion;

		freeSize = sizeOfRegion;
		return 0;
	}else {
		m_error = E_BAD_ARGS;
		return -1;
	}
}

void *Mem_Alloc(int size)
{
	struct mem_node *biggestNode  = freeHead;
	struct mem_node *currentNode  = freeHead;

	if(size % 8 != 0)
		size += 8 - (size % 8);

	if(freeHead == NULL)
	{
		//our list is empty
		m_error = E_NO_SPACE;
		return NULL;
	}

	//find the biggest free chunk (worst fit)
	do{
		if(currentNode->size > biggestNode->size)
			biggestNode = currentNode;

		currentNode = currentNode->next;
	}while(currentNode != NULL);

	struct mem_node* biggestNodePrev = biggestNode->prev;
	struct mem_node* biggestNodeNext = biggestNode->next;
	int biggestNodeSize = biggestNode->size;

	//check for freespace
	if(biggestNode->size < size + 24)
	{
		m_error = E_NO_SPACE;
		return NULL;
	}else if(biggestNode->size == size + 24) {
		if(biggestNode != freeHead)
		{
			biggestNode->prev->next = biggestNodeNext;
		}
		if(biggestNodeNext != NULL)
		{
			biggestNode->next->prev = biggestNodePrev;
		}
		if(biggestNode == freeHead)
		{
			freeHead = biggestNodeNext;
		}

		uintptr_t *new_size = (uintptr_t*)biggestNode;
		*new_size = size;
		return (void*)new_size + 24;
	}else {
		void *returnVal;
		uintptr_t *new_size = (uintptr_t*)biggestNode;
		returnVal =  (void*)new_size + 24;

		struct mem_node* newNode = returnVal + size;
		
		newNode->prev = biggestNodePrev;
		newNode->next = biggestNodeNext;
		newNode->size = biggestNodeSize - (size + 24);

		if(biggestNode != freeHead)
		{
			biggestNode->prev->next = newNode;
		}
		if(biggestNode->next != NULL)
		{
			biggestNode->next->prev = newNode;
		}
		if(biggestNode == freeHead)
		{
			freeHead = newNode;
		}

		*new_size = size;
		return returnVal;
	}


}

int Mem_Free(void *ptr)
{
	if(ptr == NULL)
		return 0;
	
	int *size = ptr - 24;

	struct mem_node *beforeNode = freeHead;
	struct mem_node *newNode = (struct mem_node*)size;

	if(beforeNode == NULL)
	{
		//Freed chunk size of our entire init size
		
		newNode->size = *size + 24;
		newNode->prev = NULL;
		newNode->next = NULL;
		freeHead = newNode;
		return 0;
	}else if((uintptr_t*)size < (uintptr_t*)freeHead)
	{
		//Freed memory at beginning of list

		newNode->size = *size + 24;
		newNode->prev = NULL;
		newNode->next = freeHead;
		freeHead->prev = newNode;
		freeHead = newNode;
	}else {

		while((void*)beforeNode->next < (void*)size && beforeNode->next != NULL)
		{
			beforeNode = beforeNode->next;
		}

		if (beforeNode == freeHead)
		{
			newNode->size = *size + 24;
			newNode->prev = beforeNode;
			newNode->next = beforeNode->next;
			newNode->next->prev = newNode;
			beforeNode->next = newNode;
		}
		else if(beforeNode->next == NULL)
		{
			newNode->size = *size + 24;
			newNode->prev = beforeNode;
			newNode->prev->next = newNode;
			newNode->next = beforeNode->next;
			beforeNode->next = newNode;
		}
		else if(beforeNode != freeHead)
		{
			newNode->size = *size + 24;
			newNode->prev = beforeNode;
			newNode->next = beforeNode->next;
			beforeNode->next = newNode;
			newNode->next->prev = newNode;
		}
	}

	consolidateAfter(consolidateBefore(newNode));
	return 0;
}

struct mem_node* consolidateBefore(struct mem_node *newNode)
{
	struct mem_node *prevNode = newNode->prev;
	if(prevNode != NULL)
	{
		if(((void*)prevNode + prevNode->size) == newNode)
		{
			prevNode->next = newNode->next;
			prevNode->size += newNode->size;
			prevNode->next->prev = prevNode;
			return prevNode;
		}
	}
	return newNode;
}

struct mem_node* consolidateAfter(struct mem_node *newNode)
{
	struct mem_node *nextNode = newNode->next;
	if(nextNode != NULL)
	{
		if(((void*)newNode + newNode->size) == nextNode)
		{
			newNode->next = nextNode->next;
			if(newNode->next != NULL)
				newNode->next->prev = newNode;

			newNode->size += nextNode->size;
		}
	}
	return newNode;
}

void Mem_Dump(){
	struct mem_node *nextNode = freeHead;
	while(nextNode != NULL)
	{
		printf("Node at: %p\n\tPrevious: %p\n\tNext: %p\n\tSize: %d\n", nextNode, nextNode->prev, nextNode->next, nextNode->size);
		nextNode = nextNode->next;
	}
}