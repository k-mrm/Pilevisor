#ifndef DEVICE_H
#define DEVICE_H

#include "types.h"
#include "compiler.h"

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

#define foreach_device_node_child(n, node)   \
  for(n = node->child; n; n = n->next)

void device_tree_init(void *fdt_base);

struct device_node *dt_node_alloc(struct device_node *parent);
struct property *dt_prop_alloc(struct device_node *node);

int dt_node_propa(struct device_node *node, const char *name, u32 *buf);
const char *dt_node_props(struct device_node *node, const char *name);
bool dt_node_propb(struct device_node *node, const char *name);

struct device_node *dt_find_node_type(struct device_node *node, const char *type);
struct device_node *dt_find_node_type_cont(struct device_node *node, const char *type,
                                            struct device_node *cont);
struct device_node *dt_find_node_path(struct device_node *node, const char *path);

struct dt_compatible {
  const char *comp;
};

struct dt_device {
  struct dt_compatible *compat;
  void (*init)(struct device_node *);
};

#define DT_IRQCHIP_INIT(name, comp, initfn)                     \
  static struct dt_device _dt_irqchip_ ## name                  \
    __used __section("__dt_irqchip_device") __aligned(8) = {    \
    .compat = comp,                                             \
    .init = initfn,                                             \
  };

extern struct dt_device __dt_irqchip_device[];

#endif
