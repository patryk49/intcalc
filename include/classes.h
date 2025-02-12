#pragma once

#include "utils.h"
#include "structs.h"





// IDENTIFIER STUFF
static uint64_t name_hash(const char *str, size_t length){
	// djb2 hash
	uint64_t hash = 5381;
	for (size_t i=0; i!=length; i+=1){
		hash = ((hash << 5u) + hash) + (uint64_t)str[i];
	}
	return hash;
}

struct NameEntry{
	uint64_t hash;
	NameId   name_id : 24;
	size_t   length  : 8;
	uint32_t data_index;
};

struct GlobalNameSet{
	struct NameEntry *data;
	size_t size     : 32;
	size_t capacity : 32;
};

struct GlobalNameData{
	uint8_t *data;
	size_t   size     : 32;
	size_t   capacity : 32;
};


static struct GlobalNameSet  global_name_set;
static struct GlobalNameData global_names;

static size_t hash_colissions = 0; 

static NameId get_name_id(const char *str, uint8_t length){
	assert(util_is_power2_u32(global_name_set.capacity));
	assert(length != 0 && length <= 255);

	struct GlobalNameSet  name_set = global_name_set;
	struct GlobalNameData names    = global_names;

	uint64_t hash = name_hash(str, length);
	size_t index_mask = name_set.capacity - 1;
	size_t index = hash & index_mask;
	for (size_t i=0;; i+=1){
		struct NameEntry entry = name_set.data[index];
		if (entry.length == 0) break; // name not found
		if (entry.hash == hash && entry.length == length){
			if (memcmp(names.data + entry.name_id, str, length) == 0){
				return entry.name_id;
			}
			hash_colissions += 1;
		}
		index = (index + i + 1) & index_mask;
	}
	
	// add new entry's name data
	NameId result = names.size + 1;
	size_t new_names_size = names.size + length + 1;
	if (new_names_size > names.capacity){
		size_t   new_names_capacity = 2*names.capacity;
		uint8_t *new_names_data = malloc(new_names_capacity);
		assert(new_names_data != NULL && "name allocation failrule");
		memcpy(new_names_data, names.data, names.size);
		free(names.data);
		names.data     = new_names_data;
		names.capacity = new_names_capacity;
		global_names = names;
	}
	names.data[names.size] = length;
	memcpy(names.data+names.size+1, str, length);
	global_names.size = new_names_size;
	
	// add new entry to set
	name_set.data[index] = (struct NameEntry){
		.hash    = hash,
		.name_id = result,
		.length  = length
	};
	name_set.size += 1;
	global_name_set.size = name_set.size;
	
	UNLIKELY if (4*name_set.size >= 3*name_set.capacity){
		// resize hash table
		size_t new_hs_capacity = 2*name_set.capacity;
		struct NameEntry *new_hs_data = malloc(new_hs_capacity*sizeof(struct NameEntry));
		if (new_hs_data == NULL){
			assert(false && "name allocation failrule");
		}
		memset(new_hs_data, 0, new_hs_capacity*sizeof(struct NameEntry));
		// reindex old hash table
		size_t elem_index_mask = new_hs_capacity - 1;
		for (size_t i=0; i!=name_set.capacity; i+=1){
			struct NameEntry old_set_entry = name_set.data[i];
			if (old_set_entry.name_id != 0){
				size_t elem_index = old_set_entry.hash & elem_index_mask;
				for (size_t i=0;; i+=1){
					struct NameEntry entry = new_hs_data[elem_index];
					if (entry.length == 0) break; // add elem to new table
					elem_index = (elem_index + i + 1) & elem_index_mask;
				}
				new_hs_data[elem_index] = old_set_entry;
			}
		}
		free(name_set.data);
		global_name_set.data     = new_hs_data;
		global_name_set.capacity = new_hs_capacity;
	}
	return result;
}





// INITIALIZING GLOBAL VARIABLES
static void initialize_compiler_globals(void){
	// name set
	global_name_set.capacity = 128;
	global_name_set.size = 0;
	size_t name_set_bytes = global_name_set.capacity*sizeof(struct NameEntry);
	global_name_set.data = malloc(name_set_bytes);
	assert(global_name_set.data != NULL);
	memset(global_name_set.data, 0, name_set_bytes);
	
	// name data 
	global_names.capacity = global_name_set.capacity*(1+8);
	global_names.size = 0;
	global_names.data = malloc(global_names.capacity);
	assert(global_names.data != NULL);

	// keywords & directires
	init_keyword_names();
}


