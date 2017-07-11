/*
 * Copyright (c) 2016-2017 Wuklab, Purdue University. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef _LEGO_HASHTABLE_H_
#define _LEGO_HASHTABLE_H_

#include <lego/list.h>
#include <lego/kernel.h>

#define DEFINE_HASHTABLE(name, bits)                                            \
        struct hlist_head name[1 << (bits)] =                                   \
                                { [0 ... ((1 << (bits)) - 1)] = HLIST_HEAD_INIT }

#define HASH_SIZE(name) (ARRAY_SIZE(name))

static inline void __hash_init(struct hlist_head *ht, unsigned int sz)
{
        unsigned int i;
        
        for (i = 0; i < sz; i++)
                INIT_HLIST_HEAD(&ht[i]);
}

/*
 * hash_init - initialize a hash table
 * @hashtable: hashtable to be initialized
 */
#define hash_init(hashtable) __hash_init(hashtable, HASH_SIZE(hashtable))

/* 
 * hash_add - adds the node to the hash table
 * @hashtable: hashtable where the node is to be added
 * @node: &struct hlist_node to be added
 * @key : key for the object
 */ 
#define hash_add(hashtable, node, key)                                          \
        hlist_add_head(node, &hashtable[hash_min(key, HASH_BITS(hashtable))])

#define hash_remove(node)       hlist_del(node)

#define GOLDEN_RATIO_32 0x61C88647
#define GOLDEN_RATIO_64 0x61C8864680B583EBull

#define hash_long(val, bits) hash_64_generic(val, bits)

static inline bool __hash_empty(struct hlist_head *ht, unsigned int sz)
{
        unsigned int i;

        for (i = 0; i < sz; i++)
                if (!hlist_empty(&ht[i]))
                        return false;

        return true;
}

static __always_inline u32 hash_64_generic(u64 val, unsigned int bits)
{
        /* 64x64-bit multiply is efficient on all 64-bit processors */
        return val * GOLDEN_RATIO_64 >> (64 - bits);
}
                                
/**
 *  hash_empty - check whether a hashtable is empty
 *  @hashtable: hashtable to check
 */
#define hash_empty(hashtable) __hash_empty(hashtable, HASH_SIZE(hashtable))

/*
 * hash_del - remove an object from a hashtable
 * @node: &struct hlist_node of the object to remove
 */
static inline void hash_del(struct hlist_node *node)
{ 
        hlist_del_init(node);
}

/**
 * hash_for_each_possible - iterate over all possible objects hashing to the
 * same bucket
 * @name: hashtable to iterate
 * @obj: the type * to use as a loop cursor for each entry
 * @member: the name of the hlist_node within the struct
 * @key: the key of the objects to iterate over
 */
#define hash_for_each_possible(name, obj, member, key)                  \
        hlist_for_each_entry(obj, &name[hash_min(key, HASH_BITS(name))], member)

#endif /* _LEGO_HASHTABLE_H_ */
