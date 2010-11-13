
// uthash-1.9.2 is distributed with the Avon source
#include "uthash-1.9.2/src/utarray.h"
#include "uthash-1.9.2/src/uthash.h"
#include "uthash-1.9.2/src/utstring.h"

// static allocation for model names makes the code very simple, but
// will fail/crash if names exceed these lengths
#define NAME_LEN_MAX 512
#define TYPE_LEN_MAX 512

// not for users
typedef struct {
  char id[NAME_LEN_MAX];  /* model name and hash table key */          
	char prototype[NAME_LEN_MAX]; /* clue to clients about what kind of
																	 object this is.*/
	av_interface_t interface; /* specifies which message handlers are
															 called for this model */
  UT_hash_handle hh; /* makes this structure hashable */
  UT_array* children; /* array of strings naming our children */
} _av_node_t;
