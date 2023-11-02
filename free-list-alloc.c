#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  size_t block_size;
  size_t padding;
} FreeListAllocationHeader;

typedef struct FreeListNode {
  struct FreeListNode *next;
  size_t block_size;
} FreeListNode;

typedef enum {
  PlacementPolicyFindFirst,
  PlacementPolicyFindBest,
} PlacementPolicy;

typedef struct {
  void *data;
  size_t size;
  size_t used;

  FreeListNode *head;
  PlacementPolicy policy;
} FreeList;

void free_list_free_all(FreeList *fl) {
  fl->used = 0;
  FreeListNode *first_node = (FreeListNode *)fl->data;
  first_node->block_size = fl->size;
  first_node->next = NULL;
  fl->head = first_node;
}

void free_list_init(FreeList *fl, void *data, size_t size) {
  fl->data = data;
  fl->size = size;
  free_list_free_all(fl);
}

bool is_power_of_two(uintptr_t ptr) { return (ptr & (ptr - 1)) == 0; }

size_t calc_padding_with_header(uintptr_t ptr, uintptr_t alignment,
                                size_t header_size) {
  assert(is_power_of_two(alignment));

  uintptr_t a = alignment;
  uintptr_t modulo = ptr & (a - 1);

  uintptr_t padding = 0;
  uintptr_t needed_space = (uintptr_t)header_size;

  if (modulo != 0) {
    padding = a - modulo;
  }

  if (padding < needed_space) {
    needed_space -= padding;

    if ((needed_space & (a - 1)) != 0) { // needed_space % a != 0
      padding += a * (1 + (needed_space / a));
    } else {
      padding += a * (needed_space / a);
    }
  }

  return (size_t)padding;
}

FreeListNode *free_list_find_first(FreeList *fl, size_t size, size_t alignment,
                                   size_t *padding_,
                                   FreeListNode **prev_node_) {
  FreeListNode *node = fl->head;
  FreeListNode *prev_node = NULL;

  size_t padding = 0;

  while (node != NULL) {
    padding = calc_padding_with_header((uintptr_t)node, (uintptr_t)alignment,
                                       sizeof(FreeListAllocationHeader));
    size_t required_space = size + padding;

    if (node->block_size >= required_space) {
      break;
    }
    prev_node = node;
    node = node->next;
  }

  if (padding_) {
    *padding_ = padding;
  }

  if (prev_node_) {
    *prev_node_ = prev_node;
  }

  return node;
}

FreeListNode *free_list_find_best(FreeList *fl, size_t size, size_t alignment,
                                  size_t *padding_, FreeListNode **prev_node_) {

  size_t smallest_diff = ~(size_t)0;

  FreeListNode *node = fl->head;
  FreeListNode *prev_node = NULL;
  FreeListNode *best_node = NULL;

  size_t padding = 0;

  while (node != NULL) {
    padding = calc_padding_with_header((uintptr_t)node, (uintptr_t)alignment,
                                       sizeof(FreeListAllocationHeader));

    size_t required_space = size + padding;

    if (node->block_size >= required_space &&
        (node->block_size - required_space < smallest_diff)) {
      best_node = node;
    }

    prev_node = node;
    node = node->next;
  }

  if (padding_) {
    *padding_ = padding;
  }

  if (prev_node_) {
    *prev_node_ = prev_node;
  }

  return best_node;
}

void free_list_node_insert(FreeListNode **head, FreeListNode *prev,
                           FreeListNode *new) {
  if (!prev) {
    if (*head != NULL) {
      new->next = *head;
    } else {
      *head = new;
    }
  } else {
    if (!prev->next) {
      prev->next = new;
      new->next = NULL;
    } else {
      new->next = prev->next;
      prev->next = new;
    }
  }
}

void free_list_node_remove(FreeListNode **head, FreeListNode *prev,
                           FreeListNode *del) {
  if (!prev) {
    *head = del->next;
  } else {
    prev->next = del->next;
  }
}

void *free_list_alloc(FreeList *fl, size_t size, size_t alignment) {
  size_t padding = 0;

  FreeListNode *prev_node = NULL;
  FreeListNode *node = NULL;
  FreeListAllocationHeader *header_ptr;

  if (size < sizeof(FreeListNode)) {
    size = sizeof(FreeListNode);
  }

  if (alignment < 8) {
    alignment = 8;
  }

  switch (fl->policy) {
  case PlacementPolicyFindFirst:
    node = free_list_find_first(fl, size, alignment, &padding, &prev_node);
    break;
  case PlacementPolicyFindBest:
    node = free_list_find_best(fl, size, alignment, &padding, &prev_node);
    break;
  }

  if (!node) {
    return NULL; // No free memory
  }

  size_t alignment_padding = padding - sizeof(FreeListAllocationHeader);
  size_t required_space = size + padding;
  size_t remaining = node->block_size - required_space;

  if (remaining > 0) {
    FreeListNode *new_node = (FreeListNode *)((char *)node + required_space);
    new_node->block_size = remaining;
    free_list_node_insert(&fl->head, node, new_node);
  }

  free_list_node_remove(&fl->head, prev_node, node);

  header_ptr = (FreeListAllocationHeader *)((char *)node + alignment_padding);
  header_ptr->block_size = required_space;
  header_ptr->padding = alignment_padding;

  fl->used += required_space;

  return (void *)((char *)header_ptr + sizeof(FreeListAllocationHeader));
}

void free_list_node_coalesce(FreeList *fl, FreeListNode *prev, FreeListNode *free) {
    if (free->next != NULL && (void *)((char*)free + free->block_size) == free->next) {
        free->block_size += free->next->block_size;
        free_list_node_remove(&fl->head, free, free->next);
    }

    if (prev->next != NULL && (void *)((char *)prev + prev->block_size) == free) {
        prev->block_size += free->block_size;
        free_list_node_remove(&fl->head, prev, free);
    }
}

void free_list_free(FreeList *fl, void *ptr) {
    if (ptr == NULL) {
        return;
    }

    FreeListAllocationHeader *header = (FreeListAllocationHeader *)((char *)ptr - sizeof(FreeListAllocationHeader));
    FreeListNode *prev_node = NULL;
    FreeListNode *free_node = (FreeListNode *)header;
    free_node->block_size = header->block_size + header->padding;
    free_node->next = NULL;

    FreeListNode *node = fl->head;
    while (node != NULL) {
        if (ptr < (void *)node) {
            free_list_node_insert(&fl->head, prev_node, node);
        }

        prev_node = node;
        node = node->next;
    }

    fl->used -= free_node->block_size;

    free_list_node_coalesce(fl, prev_node, free_node);
}
