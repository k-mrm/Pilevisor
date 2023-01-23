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

  u32 phandle;
  const char *name;
  const char *device_type;
  struct property *prop;
};

struct dt_compatible {
  const char *comp;
};

struct dt_device {
  char *dev_name;
  struct dt_compatible *compat;
  int (*init)(struct device_node *);
};

#define foreach_device_node_child(n, node)   \
  for(n = node->child; n; n = n->next)

void device_tree_init(void *fdt_base);

struct device_node *dt_node_alloc(struct device_node *parent);
struct property *dt_prop_alloc(struct device_node *node);

int dt_node_propa(struct device_node *node, const char *name, u32 *buf);
int dt_node_propa64(struct device_node *node, const char *name, u64 *buf);
const char *dt_node_props(struct device_node *node, const char *name);
bool dt_node_propb(struct device_node *node, const char *name);
int dt_node_prop_addr(struct device_node *node, int index, u64 *addr, u64 *size);
bool dt_node_props_is(struct device_node *node, const char *name, const char *str);

struct device_node *dt_find_node_type_cont(const char *type, struct device_node *cont);

static inline struct device_node *dt_find_node_type(const char *type) {
  return dt_find_node_type_cont(type, NULL);
}

struct device_node *dt_find_node_path(const char *path);

int compat_dt_device_init(struct dt_device *table, struct device_node *node,
                           const char *compat);

struct dt_device *dt_compatible_device(struct dt_device *table, struct device_node *node);
struct device_node *next_match_node(struct dt_device *table, struct dt_device **dev,
                                    struct device_node *prev);

bool dt_node_device_type_is(struct device_node *node, const char *type);

struct device_node *dt_next_cpu_device(struct device_node *prev);

#define DT_IRQCHIP_INIT(name, comp, initfn)                                             \
  static const struct dt_device _dt_irqchip_ ## name                                    \
    __used __section("__dt_irqchip_device") __aligned(_Alignof(struct dt_device)) = {   \
    .dev_name = #name,                                                                  \
    .compat = comp,                                                                     \
    .init = initfn,                                                                     \
  };

#define DT_SERIAL_INIT(name, comp, initfn)                                              \
  static const struct dt_device _dt_serial_ ## name                                     \
    __used __section("__dt_serial_device") __aligned(_Alignof(struct dt_device)) = {    \
    .dev_name = #name,                                                                  \
    .compat = comp,                                                                     \
    .init = initfn,                                                                     \
  };

extern struct dt_device __dt_irqchip_device[];
extern struct dt_device __dt_serial_device[];

#endif
