#include "types.h"
#include "device.h"
#include "malloc.h"
#include "fdt.h"

struct device_node *device_node_alloc(struct device_node *parent) {
  struct device_node *node = malloc(sizeof(*node));
  if(!node)
    panic("nomem");

  node->parent = parent;
  node->child = NULL;

  if(parent) {
    node->next = parent->child;
    parent->child = node;
  }

  return node;
}

struct property *device_property_alloc(struct device_node *node) {
  struct property *p = malloc(sizeof(*p));
  if(!p)
    panic("nomem");

  p->next = node->prop;
  node->prop = p;

  return p;
}

struct device_node *device_tree_init(void *fdt_base) {
  struct fdt fdt;

  fdt_probe(&fdt, fdt_base);

  struct device_node *node = fdt_parse(&fdt);

  return node;
}
