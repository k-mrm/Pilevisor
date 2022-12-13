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
  const char *device_type;
  struct property *prop;
};

void device_tree_init(void *fdt_base);

struct device_node *dt_node_alloc(struct device_node *parent);
struct property *dt_prop_alloc(struct device_node *node);

int dt_node_propa(struct device_node *node, const char *name, u32 *buf);
const char *dt_node_props(struct device_node *node, const char *name);
struct device_node *dt_find_node_type(struct device_node *node, const char *type);
struct device_node *dt_find_node_path(struct device_node *node, const char *path);

#endif
