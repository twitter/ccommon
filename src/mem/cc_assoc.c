#include "cc_assoc.h"

#include "cc_items.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>

struct node {
    struct node *next;
    struct node *prev;
    struct item *it;
};

struct node *head;

/*
 * Print all keys that are currently stored. Can be useful for debugging.
 */
void
assoc_print_all(void)
{
    struct node *iter;

    for(iter = head; iter != NULL; iter = iter->next) {
	printf("key: %s\n", item_key(iter->it));
    }
}

rstatus_t
assoc_init(void)
{
    head = NULL;
    return CC_OK;
}

void
assoc_deinit(void)
{
    struct node *next;

    for(; head != NULL; head = next) {
	next = head->next;
	free(head);
    }
}

static struct node *get_node(const char *key, size_t nkey) {
    struct node *iter;

    assert(key != NULL && nkey != 0);

    for(iter = head; iter != NULL; iter = iter->next) {
	if(memcmp(key, item_key(iter->it), nkey) == 0 && nkey == iter->it->nkey) {
	    /* Match found */
	    return iter;
	}
    }
    return NULL;
}

struct item *assoc_find(const char *key, size_t nkey) {
    struct node *node = get_node(key, nkey);

    if(node != NULL) {
	return node->it;
    }

    return NULL;
}

void assoc_insert(struct item *item) {
    struct node *new_node = malloc(sizeof(struct node));

    assert(assoc_find(item_key(item), item->nkey) == NULL);

    new_node->next = head;
    new_node->prev = NULL;
    if(head != NULL) {
	head->prev = new_node;
    }
    new_node->it = item;
    head = new_node;
}

void assoc_delete(const char *key, size_t nkey) {
    struct node *node = get_node(key, nkey);

    if(node == NULL) {
	fprintf(stderr, "error: item with key %s not found!\n", key);
	return;
    }

    if(node == head) {
	head = node->next;
    }

    if(node->prev != NULL) {
	node->prev->next = node->next;
    }

    if(node->next != NULL) {
	node->next->prev = node->prev;
    }

    free(node);
}
