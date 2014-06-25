#include "cc_interface.h"
#include "cc_settings.h"
#include "cc_assoc.h"
#include "cc_slabs.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

void init_settings(void);

void
init_settings(void) {
    settings.prealloc = true;
    settings.evict_lru = true;
    settings.use_freeq = true;
    settings.use_cas = false;
    settings.maxbytes = 8448;
    settings.slab_size = 1056;  
    settings.profile[1] = 128;
    settings.profile[2] = 256;
    settings.profile[3] = 512;
    settings.profile[4] = 1024;
    settings.profile_last_id = 4;
    settings.oldest_live = 6000;
}

int 
main(void) 
{
    rstatus_t return_status;
    char *val, *buf;

    init_settings();
    time_init();
    item_init();

    return_status = assoc_init();
    if(return_status != CC_OK) {
	fprintf(stderr, "fatal: assoc_init failed! error code %d\n", 
		return_status);
	return 1;
    }

    return_status = slab_init();
    if(return_status != CC_OK) {
	fprintf(stderr, "fatal: slab_init failed! error code %d\n",
		return_status);
	return 1;
    }

    /* Store two key/value pairs, retrieve them */
    printf("@@@ Storing two key/val pairs @@@\n");
    store_key_val("foo", 3, "bar", 3);
    store_key_val("foobar", 6, "foobarfoobar", 12);

    val = get_val("foo", 3);
    printf("foo val: %s\n", val);
    assert(strcmp(val, "bar") == 0);
    free(val);

    val = get_val("foobar", 6);
    printf("foobar val: %s\n", val);
    assert(strcmp(val, "foobarfoobar") == 0);
    free(val);
    
    /* Replace value for key foobar (should go through, since foobar is already
       in the server) */
    printf("@@@ Replacing value for key foobar @@@\n");
    replace_key_val("foobar", 6, "baz", 3);
    val = get_val("foobar", 6);
    printf("foobar val: %s\n", val);
    assert(strcmp(val, "baz") == 0);
    free(val);

    /* Add value for key foobar (should not go through, since foobar is already
       in the server) */
    printf("@@@ Adding value for key foobar @@@\n");
    add_key_val("foobar", 6, "qux", 3);
    val = get_val("foobar", 6);
    printf("foobar val: %s\n", val);
    assert(strcmp(val, "baz") == 0);
    free(val);

    /* Replace value for key baz (should not go through, since baz is not yet in
       the server) */
    printf("@@@ Replacing value for key baz @@@\n");
    replace_key_val("baz", 3, "qux", 3);
    val = get_val("baz", 3);
    assert(val == NULL);

    /* Add value for key baz (should go through, since baz is not yet in the 
       server) */
    printf("@@@ Adding value for key baz @@@\n");
    add_key_val("baz", 3, "qux", 3);
    val = get_val("baz", 3);
    printf("baz val: %s\n", val);
    assert(strcmp(val, "qux") == 0);
    free(val);

    /* Append value to key foo */
    printf("@@@ Appending balue for key foo @@@\n");
    append_val("foo", 3, "foofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoo", 111);
    val = get_val("foo", 3);
    printf("foo val: %s\n", val);
    assert(strcmp(val, "barfoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoo") == 0);
    free(val);

    /* Append value to key foo that will require rechaining */
    printf("@@@ Appending value to key foo that will require rechaining @@@\n");
    buf = malloc(940 * sizeof(char));
    memset(buf, 'o', 940);
    append_val("foo", 3, buf, 940);
    val = get_val("foo", 3);
    printf("foo val: %s\n", val);
    assert(strcmp(val, "barfoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo") == 0);
    free(val);

    /* Prepend value to key baz */
    printf("@@@ Prepending value to key baz @@@\n");
    prepend_val("baz", 3, "foobarbaz", 9);
    val = get_val("baz", 3);
    printf("baz val: %s\n", val);
    assert(strcmp(val, "foobarbazqux") == 0);
    free(val);

    /* Prepend value to key baz that will require reallocating item */
    printf("@@@ Prepending value to key baz that will require reallocating item @@@\n");
    prepend_val("baz", 3, "foobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobar", 96);
    val = get_val("baz", 3);
    printf("baz val: %s\n", val);
    assert(strcmp(val, "foobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarbazqux") == 0);
    free(val);

    /* Prepend value to key baz that will require rechaining */
    printf("@@@ Prepending value to key baz that will require rechaining @@@\n");
    prepend_val("baz", 3, buf, 940);
    val = get_val("baz", 3);
    printf("baz val: %s\n", val);
    assert(strcmp(val, "oooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooofoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarfoobarbazqux") == 0);
    free(val);

    /* Append value to key foo that is already chained and will require 
       rechaining */
    printf("@@@ Appending value to key foo that is already chained and will require rechaining @@@\n");
    append_val("foo", 3, buf, 940);
    val = get_val("foo", 3);
    printf("foo val: %s\n", val);
    assert(strcmp(val, "barfoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoofoooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooooo") == 0);
    free(val);

    /* Deleting key/val pair with key foo */
    printf("@@@ Deleting key/value pair with key foo @@@\n");
    delete_key_val("foo", 3);
    val = get_val("foo", 3);
    assert(val == NULL);

    return 0;
}
