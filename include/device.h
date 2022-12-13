#ifndef DEVICE_H
#define DEVICE_H

#include "types.h"

struct property {
  struct property *next;
  const char *name;
  void *data;
  u32 data_len;
};

struct device_node {
  struct device_node *parent;
  struct device_node *child;
  struct device_node *next;

  const char *name;
  struct property *prop;
};

struct device_node *device_tree_init(void *fdt_base);

struct device_node *device_node_alloc(struct device_node *parent);
struct property *device_property_alloc(struct device_node *node);

#endif
