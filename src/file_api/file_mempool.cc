//--------------------------------------------------------------------------
// Copyright (C) 2014-2016 Cisco and/or its affiliates. All rights reserved.
// Copyright (C) 2013-2013 Sourcefire, Inc.
//
// This program is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License Version 2 as published
// by the Free Software Foundation.  You may not use, modify or distribute
// this program under any other version of the GNU General Public License.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License along
// with this program; if not, write to the Free Software Foundation, Inc.,
// 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
//--------------------------------------------------------------------------
/*
 **  Author(s):  Hui Cao <huica@cisco.com>
 **
 **  NOTES
 **  5.25.13 - Initial Source Code. Hui Cao
 **
 **
 */

#include "file_mempool.h"

#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#include "log/messages.h"
#include "utils/util.h"

/*This magic is used for double free detection*/

#define FREE_MAGIC    0x2525252525252525
typedef uint64_t MagicType;

#ifdef DEBUG_MSGS
void FileMemPool::verify()
{
    uint64_t free_size;
    uint64_t release_size;

    free_size = cbuffer_used(free_list);
    release_size = cbuffer_used(released_list);

    if (free_size > cbuffer_size(free_list))
    {
        ErrorMessage("%s(%d) file_mempool: failed to verify free list!\n",
            __FILE__, __LINE__);
    }

    if (release_size > cbuffer_size(released_list))
    {
        ErrorMessage("%s(%d) file_mempool: failed to verify release list!\n",
            __FILE__, __LINE__);
    }

    /* The free mempool and size of release mempool should be smaller than
     * or equal to the size of mempool
     */
    if (free_size + release_size > total)
    {
        ErrorMessage("%s(%d) file_mempool: failed to verify mempool size!\n",
            __FILE__, __LINE__);
    }
}

#endif

void FileMemPool::free_pools()
{
    if (datapool != nullptr)
    {
        snort_free(datapool);
        datapool = nullptr;
    }

    cbuffer_free(free_list);
    cbuffer_free(released_list);
}

/*
 * Purpose: initialize a FileMemPool object and allocate memory for it
 * Args:
 *   num_objects - number of items in this pool
 *   obj_size    - size of the items
 */

FileMemPool::FileMemPool(uint64_t num_objects, size_t o_size)
{
    unsigned int i;

    if ((num_objects < 1) || (o_size < 1))
        return;

    obj_size = o_size;

    // this is the basis pool that represents all the *data pointers in the list
    datapool = (void**)snort_calloc(num_objects, obj_size);

    /* sets up the memory list */
    free_list = cbuffer_init(num_objects);
    if (!free_list)
    {
        ErrorMessage("%s(%d) file_mempool: Failed to init free list\n",
            __FILE__, __LINE__);
        free_pools();
        return;
    }

    released_list = cbuffer_init(num_objects);
    if (!released_list)
    {
        ErrorMessage("%s(%d) file_mempool: "
            "Failed to init release list\n", __FILE__, __LINE__);
        free_pools();
        return;
    }

    total = 0;
    for (i=0; i<num_objects; i++)
    {
        void* data = ((char*)datapool) + (i * obj_size);

        if (cbuffer_write(free_list,  data))
        {
            ErrorMessage("%s(%d) file_mempool: "
                "Failed to add to free list\n",
                __FILE__, __LINE__);
            free_pools();
            return;
        }
        *(MagicType*)data = FREE_MAGIC;
        total++;
    }
}

/*
 * Destroy a set of FileMemPool objects
 *
 */
FileMemPool::~FileMemPool()
{
    free_pools();
}

/*
 * Allocate a new object from the FileMemPool
 *
 * Args:
 *   FileMemPool: pointer to a FileMemPool struct
 *
 * Returns: a pointer to the FileMemPool object on success, nullptr on failure
 */

void* FileMemPool::m_alloc()
{
    void* b = nullptr;

    std::lock_guard<std::mutex> lock(pool_mutex);

    if (cbuffer_read(free_list, &b))
    {
        if (cbuffer_read(released_list, &b))
        {
            return nullptr;
        }
    }

    if (*(MagicType*)b != FREE_MAGIC)
    {
        ErrorMessage("%s(%d) file_mempool_alloc(): Allocation errors! \n",
            __FILE__, __LINE__);
    }

    DEBUG_WRAP(verify(); );

    return b;
}

/*
 * Free a new object from the buffer
 * We use circular buffer to synchronize one reader and one writer
 */
int FileMemPool::remove(CircularBuffer* cb, void* obj)
{
    if (obj == nullptr)
        return FILE_MEM_FAIL;

    if (cbuffer_write(cb, obj))
    {
        return FILE_MEM_FAIL;
    }

    if (*(MagicType*)obj == FREE_MAGIC)
    {
        DEBUG_WRAP(ErrorMessage("%s(%d) file_mempool_remove(): Double free! \n",
                __FILE__, __LINE__); );
        return FILE_MEM_FAIL;
    }

    *(MagicType*)obj = FREE_MAGIC;

    return FILE_MEM_SUCCESS;
}

int FileMemPool::m_free(void* obj)
{
    std::lock_guard<std::mutex> lock(pool_mutex);

    int ret = remove(free_list, obj);

    DEBUG_WRAP(verify(); );

    return ret;
}

/*
 * Release a new object from the FileMemPool
 * This can be called by a different thread calling
 * file_mempool_alloc()
 *  *
 */

int FileMemPool::m_release(void* obj)
{
    std::lock_guard<std::mutex> lock(pool_mutex);

    /*A writer that might from different thread*/
    int ret = remove(released_list, obj);

    DEBUG_WRAP(verify(); );

    return ret;
}

/* Returns number of elements allocated in current buffer*/
uint64_t FileMemPool::allocated()
{
    uint64_t total_freed = released() + freed();
    return (total - total_freed);
}

/* Returns number of elements freed in current buffer*/
uint64_t FileMemPool::freed()
{
    return (cbuffer_used(free_list));
}

/* Returns number of elements released in current buffer*/
uint64_t FileMemPool::released()
{
    return (cbuffer_used(released_list));
}

