#include <assert.h>
#include <memory.h>
#include <stdlib.h>

#include "list.h"

struct list *list_new(void (*dtor)(void *buf))
{
	struct list *self;

	self = malloc(sizeof(*self));
	if (!self) {
		return NULL;
	}

	self->head = NULL;
	self->last = NULL;
	self->size = 0;
	self->dtor = dtor;

	return self;
}

void list_free(struct list *self)
{
	assert(self);

	if (!self->head || self->size == 0) {
		goto cleanup;
	}

	while (self->head != NULL) {
		list_remove(self, self->head);
	}

cleanup:
	(void)memset(self, 0, sizeof(*self));
	free(self);
}

void list_remove(struct list *self, struct node *node)
{
	struct node **tmp;

	assert(self);
	
	if (!node) {
		return;
	}

	if (node == self->head) {
		self->head = node->next;
		self->size--;
		if (self->size == 0) {
			self->last = self->head;
		}

		if (node->value && self->dtor) {
			self->dtor(node->value);
		}
		(void)memset(node, 0, sizeof(*node));
		free(node);
	}
	else {
		for (tmp = list_iter_begin(self); !list_iter_done(tmp); list_iter_continue(&tmp)) {
			if ((*tmp)->next == node) {
				(*tmp)->next = node->next;
				self->size--;
				if (node == self->last) {
					self->last = *tmp;
				}

				if (self->size == 1) {
					self->head = self->last;
				}
				else if (self->size == 0) {
					self->head = NULL;
					self->last = NULL;
				}

				if (node->value && self->dtor) {
					self->dtor(node->value);
				}
				(void)memset(node, 0, sizeof(*node));
				free(node);
				break;
			}
		}
	}
}

struct node *list_find(struct list *self, void *query, int (*cmp)(void *value, void *query))
{
	struct node **tmp;

	assert(self);
	assert(cmp);

	for (tmp = list_iter_begin(self); !list_iter_done(tmp); list_iter_continue(&tmp)) {
		if (cmp(list_iter_value(tmp), query) == 0) {
			return *tmp;
		}
	}

	return NULL;
}

struct node *list_push_front(struct list *self, void *value)
{
	assert(self);
	return list_insert_next(self, NULL, value);
}

struct node *list_push_back(struct list *self, void *value)
{
	assert(self);
	return list_insert_next(self, self->last, value);
}

void list_pop_front(struct list *self)
{
	assert(self);
	list_remove(self, self->head);
}

void list_pop_back(struct list *self)
{
	assert(self);
	list_remove(self, self->last);
}

struct node *list_insert_next(struct list *self, struct node *previous, void *value)
{
	struct node *node;

	assert(self);

	node = malloc(sizeof(*node));
	if (!node) {
		return NULL;
	}

	node->value = value;

	// new head
	if (previous == NULL) {
		node->next = self->head;
		if (self->size == 0) {
			self->last = node;
		}
		self->head = node;
		self->size++;
	}
	else {
		if (previous == self->last) {
			previous->next = node;
			node->next = NULL;
			self->last = node;
		}
		else {
			node->next = previous->next;
			previous->next = node;
		}
		self->size++;
	}

	return node;
}

struct node *list_head(struct list *self)
{
	assert(self);
	return self->head;
}

struct node *list_tail(struct list *self)
{
	assert(self);
	if (self->head) {
		return self->head->next;
	}

	return NULL;
}

struct node *list_last(struct list *self)
{
	assert(self);
	return self->last;
}

struct node *list_iterate(struct list *self, struct node *current)
{
	assert(self);

	if (current == NULL) {
		return self->head;
	}

	return current->next;
}

struct node **list_iter_begin(struct list *self)
{
	assert(self);
	return &self->head;
}

void list_iter_continue(struct node ***current)
{
	assert(current);

	*current = &(**current)->next;
}

int list_iter_done(struct node **current)
{
	assert(current);
	return !(*current);
}

void *list_iter_value(struct node **current)
{
	assert(current);
	return (*current)->value;
}

struct node *list_iter_next(struct node **current)
{
	assert(current);
	return (*current)->next;
}
