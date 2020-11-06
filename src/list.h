#ifndef JLIB_LIST_H
#define JLIB_LIST_H

#include <stddef.h> /* size_t */
#include <malloc.h>

struct node {
	void *value;
	struct node *next;
};

struct list {
	struct node *head;
	struct node *last;
	size_t size;
	void (*dtor)(void *buf);
};

/**
 * Create a new, empty list
 * 
 * dtor: Specity a way to free the values
 */
struct list *list_new(void (*dtor)(void *buf));

/**
 * Free every value in a list, every node, and the list itself
 */
void list_free(struct list *self);

/**
 * Free a value and its node, rejoin adjacent nodes
 */
void list_remove(struct list *self, struct node *node);

/**
 * Traverse the list looking for a match to query, NULL if no match. cmp should return values like strcmp
 */
struct node *list_find(struct list *self, void *query, int (*cmp)(void *value, void *query));

/**
 * Make a new node at the head location. Return the new head
 */
struct node *list_push_front(struct list *self, void *value);

/**
 * Make a new node at the tail location
 */
struct node *list_push_back(struct list *self, void *value);

/**
 * Free the first node and its value, repoint the head node
 */
void list_pop_front(struct list *self);

/**
 * Free the last node and its value, repoint the tail node
 */
void list_pop_back(struct list *self);

/**
 * Make a new node after the current node pointer in the list, if previous is NULL, insert before head
 */
struct node *list_insert_next(struct list *self, struct node *previous, void *value);

/**
 * First element
 */
struct node *list_head(struct list *self);

/**
 * Second element
 */
struct node *list_tail(struct list *self);

/**
 * Last element
 */
struct node *list_last(struct list *self);

/**
 * First item in an iteration, pass result by reference to list_iter_next
 */
struct node **list_iter_begin(struct list *self);

/**
 * Continuously get next elements until NULL is reached
 * 
 * If \a *current is NULL, then it starts at self->head
 * 
 * struct node **cursor = NULL;
 * cursor = list_begin(thelist);
 * list_iter_next(&cursor);
 */
void list_iter_continue(struct node ***current);

/**
 * Convenience for the iteration being done
 */
int list_iter_done(struct node **current);
void *list_iter_value(struct node **current);
struct node *list_iter_next(struct node **current);

#endif /* JLIB_LIST_H */
