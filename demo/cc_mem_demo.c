#include <mem/cc_mem_interface.h>
#include <mem/cc_mem_settings.h>
#include <mem/cc_slab.h>
#include <mem/cc_item.h>
#include <cc_bstring.h>
#include <cc_define.h>
#include <cc_log.h>
#include <cc_mm.h>
#include <data_structure/cc_zipmap.h>
#include <hash/cc_hash_table.h>

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

#define KB 1024
#define MB (1024 * (KB))

/*
 * List of commands
 *
 * ks [key length] [key] [val length] [val]
 * Set key: sets the value to the key
 *
 * ka [key length] [key] [val length] [val]
 * Add key: sets the value to the key, but only if the key is not already in the cache
 *
 * kr [key length] [key] [val length] [val]
 * Replace key: sets the value to the key, but only if the key is already in the cache
 *
 * kg [key length] [key]
 * Get key: gets the value associated with the given key
 *
 * kd [key length] [key]
 * Delete key: removes the key-val pair from the cache
 *
 * va [key length] [key] [val length] [val]
 * Append val: appends given val to the end of the val associated with given key
 *
 * vp [key length] [key] [val length] [val]
 * Prepend val: prepends given val to the beginning of the val associated with given key
 *
 * vi [key length] [key] [val]
 * Increment val: increments val associated with given key by given val
 *
 * vd [key length] [key] [val]
 * Decrement val: decrements val associated with given key by given val
 *
 * si [key length] [key]
 * Init secondary key: creates a zipmap with the given key
 *
 * ss [key length] [key] [secondary key length] [secondary key] [val length] [val]
 * Set secondary key: sets the value to the key/secondary key pair
 *
 * sa [key length] [key] [secondary key length] [secondary key] [val length] [val]
 * Add secondary key: sets the value to the key/secondary key pair if not already in cache
 *
 * sr [key length] [key] [secondary key length] [secondary key] [val length] [val]
 * Replace secondary key: replaces the value to the key/secondary key pair if already in cache
 *
 * sd [key length] [key] [secondary key length] [secondary key]
 * Delete secondary key: removes the secondary key/value from the cache
 *
 * sg [key length] [key] [secondary key length] [secondary key]
 * Get secondary key: gets value associated with key/secondary key
 *
 * ps
 * Print settings descriptions
 *
 * qp
 * Query prealloc setting
 *
 * qe
 * Query evict_lru setting
 *
 * qf
 * Query use_freeq setting
 *
 * qc
 * Query use_cas setting
 *
 * qb
 * Query maxbytes setting
 *
 * qs
 * Query slab_size setting
 *
 * qh
 * Query hash_power setting
 *
 * qq
 * quit
 */
void init_cache(char *config_file);
void init_settings(void);
bool get_str(uint32_t *len, char **str);
void demo_set_key(void);
void demo_add_key(void);
void demo_replace_key(void);
void demo_get_key(void);
void demo_delete_key(void);
void demo_append_val(void);
void demo_prepend_val(void);
void demo_increment_val(void);
void demo_decrement_val(void);
void demo_init_secondary(void);
void demo_set_secondary(void);
void demo_add_secondary(void);
void demo_replace_secondary(void);
void demo_delete_secondary(void);
void demo_get_secondary(void);

int main(int argc, char *argv[])
{
    if(argc < 2) {
	printf("usage: memdemo [config file]\n");
	return 1;
    }

    init_cache(argv[1]);

    while(true) {
	char first, second;

	/* fetch command */
	printf("\nccommon# ");
	while(isspace(first = getchar()));
	while(isspace(second = getchar()));

	switch(first) {
	case 'k': {
	    switch(second) {
	    case 's':
		demo_set_key();
		break;
	    case 'a':
		demo_add_key();
		break;
	    case 'r':
		demo_replace_key();
		break;
	    case 'g':
		demo_get_key();
		break;
	    case 'd':
		demo_delete_key();
		break;
	    default:
		printf("unknown command entered\n");
		while(fgetc(stdin) != '\n');
		break;
	    }
	    break;
	}
	case 'v': {
	    switch(second) {
	    case 'a':
		demo_append_val();
		break;
	    case 'p':
		demo_prepend_val();
		break;
	    case 'i':
		demo_increment_val();
		break;
	    case 'd':
		demo_decrement_val();
		break;
	    default:
		printf("unknown command entered\n");
		while(fgetc(stdin) != '\n');
		break;
	    }
	    break;
	}
	case 's': {
	    switch(second) {
	    case 'i':
		demo_init_secondary();
		break;
	    case 's':
		demo_set_secondary();
		break;
	    case 'a':
		demo_add_secondary();
		break;
	    case 'r':
		demo_replace_secondary();
		break;
	    case 'd':
		demo_delete_secondary();
		break;
	    case 'g':
		demo_get_secondary();
		break;
	    default:
		printf("unknown command entered\n");
		while(fgetc(stdin) != '\n');
		break;
	    }
	    break;
	}
	case 'p': {
	    switch(second) {
	    case 's':
		mem_settings_desc();
		break;
	    default:
		printf("unknown command entered\n");
		while(fgetc(stdin) != '\n');
		break;
	    }
	    break;
	}
	case 'q': {
	    switch(second) {
	    case 'p':
		printf(mem_settings.prealloc.val.bool_val ? "true\n" : "false\n");
		break;
	    case 'e':
		printf(mem_settings.evict_lru.val.bool_val ? "true\n" : "false\n");
		break;
	    case 'f':
		printf(mem_settings.use_freeq.val.bool_val ? "true\n" : "false\n");
		break;
	    case 'c':
		printf(mem_settings.use_cas.val.bool_val ? "true\n" : "false\n");
		break;
	    case 'b':
		printf("%llu\n", mem_settings.maxbytes.val.uint64_val);
		break;
	    case 's':
		printf("%u\n", mem_settings.slab_size.val.uint32_val);
		break;
	    case 'h':
		printf("%hhu\n", mem_settings.hash_power.val.uint8_val);
		break;
	    case 'q':
		printf("done\n");
		return 0;
	    default:
		printf("unknown command entered\n");
		while(fgetc(stdin) != '\n');
		break;
	    }
	    break;
	}
	default: {
	    printf("unknown command entered\n");
	    while(fgetc(stdin) != '\n');
	    break;
	}
	}
    }
}

void
init_cache(char *config_file)
{
    mem_settings_load_from_file(config_file);
    time_init();

    if(log_setup(LOG_WARN, "out.txt") == -1) {
	log_stderr("fatal: log_init failed!");
	exit(1);
    }

    if(item_init() != CC_OK) {
	log_stderr("fatal: item_init failed!");
	exit(1);
    }

    if(item_hash_init(0) != CC_OK) {
	log_stderr("fatal: item_hash_init failed!");
	exit(1);
    }

    if(slab_init() != CC_OK) {
	log_stderr("fatal: slab_init failed!");
	exit(1);
    }
}

bool
get_str(uint32_t *len, char **str)
{
    if(scanf("%u", len) != 1) {
	printf("Could not read an integer value\n");
	while(fgetc(stdin) != '\n');
	return false;
    }

    if((*str = malloc((*len) + 1)) == NULL) {
	printf("Not enough memory to do that\n");
	while(fgetc(stdin) != '\n');
	return false;
    }

    scanf("%s", *str);
    return true;
}

void
demo_set_key(void)
{
    uint32_t nkey, nval;
    char *key, *val;

    if(!get_str(&nkey, &key)) {
	return;
    }

    if(!get_str(&nval, &val)) {
	free(key);
	return;
    }

    store_key(key, nkey, val, nval);

    free(key);
    free(val);
}

void
demo_add_key(void)
{
    uint32_t nkey, nval;
    char *key, *val;

    if(!get_str(&nkey, &key)) {
	return;
    }

    if(!get_str(&nval, &val)) {
	free(key);
	return;
    }

    add_key(key, nkey, val, nval);

    free(key);
    free(val);
}

void
demo_replace_key(void)
{
    uint32_t nkey, nval;
    char *key, *val;

    if(!get_str(&nkey, &key)) {
	return;
    }

    if(!get_str(&nval, &val)) {
	free(key);
	return;
    }

    replace_key(key, nkey, val, nval);

    free(key);
    free(val);
}

void
demo_get_key(void)
{
    uint32_t nkey, nval;
    char *key, *val;

    if(!get_str(&nkey, &key)) {
	return;
    }

    nval = get_val_size(key, nkey);

    if((val = malloc(nval + 1)) == NULL) {
	printf("Not enough memory for that\n");
	free(key);
	return;
    }

    if(get_val(key, nkey, val, nval + 1, 0)) {
	val[nval] = '\0';
	printf("val: %s\n", val);
    } else {
	printf("get key failed\n");
    }

    free(val);
    free(key);
}

void
demo_delete_key(void)
{
    uint32_t nkey;
    char *key;

    if(!get_str(&nkey, &key)) {
	return;
    }

    remove_key(key, nkey);

    free(key);
}

void
demo_append_val(void)
{
    uint32_t nkey, nval;
    char *key, *val;

    if(!get_str(&nkey, &key)) {
	return;
    }

    if(!get_str(&nval, &val)) {
	free(key);
	return;
    }

    append_val(key, nkey, val, nval);

    free(key);
    free(val);
}

void
demo_prepend_val(void)
{
    uint32_t nkey, nval;
    char *key, *val;

    if(!get_str(&nkey, &key)) {
	return;
    }

    if(!get_str(&nval, &val)) {
	free(key);
	return;
    }

    prepend_val(key, nkey, val, nval);

    free(key);
    free(val);
}

void
demo_increment_val(void)
{
    uint32_t nkey;
    char *key;
    uint64_t val;

    if(!get_str(&nkey, &key)) {
	return;
    }

    if(scanf("%llu", &val) != 1) {
	printf("Could not read an integer\n");
	free(key);
	return;
    }

    increment_val(key, nkey, val);

    free(key);
}

void
demo_decrement_val(void)
{
    uint32_t nkey;
    char *key;
    uint64_t val;

    if(!get_str(&nkey, &key)) {
	return;
    }

    if(scanf("%llu", &val) != 1) {
	printf("Could not read an integer\n");
	free(key);
	return;
    }

    decrement_val(key, nkey, val);

    free(key);
}

void
demo_init_secondary(void)
{
    uint32_t nkey;
    char *key;

    if(!get_str(&nkey, &key)) {
	return;
    }

    zmap_init(key, nkey);

    free(key);
}

void
demo_set_secondary(void)
{
    uint32_t npkey, nskey, nval;
    char *pkey, *skey, *val;

    if(!get_str(&npkey, &pkey)) {
	return;
    }

    if(!get_str(&nskey, &skey)) {
	free(pkey);
	return;
    }

    if(!get_str(&nval, &val)) {
	free(pkey);
	free(skey);
	return;
    }

    zmap_set(pkey, npkey, skey, nskey, val, nval);

    free(val);
    free(skey);
    free(pkey);
}

void
demo_add_secondary(void)
{
    uint32_t npkey, nskey, nval;
    char *pkey, *skey, *val;

    if(!get_str(&npkey, &pkey)) {
	return;
    }

    if(!get_str(&nskey, &skey)) {
	free(pkey);
	return;
    }

    if(!get_str(&nval, &val)) {
	free(pkey);
	free(skey);
	return;
    }

    zmap_add(pkey, npkey, skey, nskey, val, nval);

    free(val);
    free(skey);
    free(pkey);
}

void
demo_replace_secondary(void)
{
    uint32_t npkey, nskey, nval;
    char *pkey, *skey, *val;

    if(!get_str(&npkey, &pkey)) {
	return;
    }

    if(!get_str(&nskey, &skey)) {
	free(pkey);
	return;
    }

    if(!get_str(&nval, &val)) {
	free(pkey);
	free(skey);
	return;
    }

    zmap_replace(pkey, npkey, skey, nskey, val, nval);

    free(val);
    free(skey);
    free(pkey);
}

void
demo_delete_secondary(void)
{
    uint32_t npkey, nskey;
    char *pkey, *skey;

    if(!get_str(&npkey, &pkey)) {
	return;
    }

    if(!get_str(&nskey, &skey)) {
	free(pkey);
	return;
    }

    zmap_delete(pkey, npkey, skey, nskey);

    free(skey);
    free(pkey);
}

void
demo_get_secondary(void)
{
    uint32_t npkey, nskey, nval;
    char *pkey, *skey, *val_cpy;
    void *val;

    if(!get_str(&npkey, &pkey)) {
	return;
    }

    if(!get_str(&nskey, &skey)) {
	free(pkey);
	return;
    }

    if(zmap_get(pkey, npkey, skey, nskey, &val, &nval) == ZMAP_GET_OK) {
	val_cpy = malloc(nval + 1);
	cc_memcpy(val_cpy, val, nval);
	val_cpy[nval] = '\0';
	printf("val: %s\n", val_cpy);
    } else {
	printf("get failed\n");
    }

    free(pkey);
    free(skey);
}
