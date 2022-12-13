#include "types.h"
#include "device.h"
#include "malloc.h"
#include "localnode.h"
#include "lib.h"
#include "fdt.h"

struct device_node *dt_node_alloc(struct device_node *parent) {
  struct device_node *node = malloc(sizeof(*node));
  if(!node)
    panic("nomem");

  node->parent = parent;
  node->child = NULL;
  node->name = NULL;
  node->prop = NULL;

  if(parent) {
    node->next = parent->child;
    parent->child = node;
  }

  return node;
}

struct property *dt_prop_alloc(struct device_node *node) {
  struct property *p = malloc(sizeof(*p));
  if(!p)
    panic("nomem");

  p->next = node->prop;
  node->prop = p;

  return p;
}

const char *dt_find_props(struct device_node *node, char *name) {
  struct property *p = node->prop;

  for(; p; p = p->next) {
    if(strcmp(p->name, name) == 0)
      return (const char *)p->data;
  }

  return NULL;
}

void dt_find_node(struct device_node *root) {
  ;
}

void device_tree_init(void *fdt_base) {
  struct fdt fdt;

  fdt_probe(&fdt, fdt_base);

  struct device_node *root = fdt_parse(&fdt);

  const char *mach = dt_find_props(root, "compatible");
  if(mach)
    printf("vmm boot on %s\n", mach);

  localnode.device_tree = root;
}
