#include "types.h"
#include "device.h"
#include "malloc.h"
#include "localnode.h"
#include "lib.h"
#include "fdt.h"
#include "memory.h"
#include "earlycon.h"

static inline struct device_node *next_node(struct device_node *prev) {
  if(!prev)
    return localnode.device_tree;

  if(prev->child)
    return prev->child;

  if(!prev->next) {
    struct device_node *n = prev->parent;

    while(n->parent && !n->next)
      n = n->parent;

    return n->next;
  }

  return prev->next;
}

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

fdt32 *dt_node_prop_raw(struct device_node *node, const char *name, u32 *len) {
  struct property *p;

  for(p = node->prop; p; p = p->next) {
    if(strcmp(p->name, name) == 0) {
      if(len)
        *len = p->data_len;
      return p->data;
    }
  }

  return NULL;
}

int dt_node_propa(struct device_node *node, const char *name, u32 *buf) {
  if(!buf)
    return -1;

  struct property *p = node->prop;

  for(; p; p = p->next) {
    if(strcmp(p->name, name) == 0) {
      fdt32 *data = p->data;
      for(int i = 0; i < p->data_len / 4; i++) {
        fdt32 d = data[i];
        buf[i] = fdt32_to_u32(d);
      }

      return 0;
    }
  }

  return -1;
}

int dt_node_propa64(struct device_node *node, const char *name, u64 *buf) {
  if(!buf)
    return -1;

  struct property *p = node->prop;

  for(; p; p = p->next) {
    if(strcmp(p->name, name) == 0) {
      fdt64 *data = p->data;
      for(int i = 0; i < p->data_len / 8; i++) {
        fdt64 d = data[i];
        buf[i] = fdt64_to_u64(d);
      }

      return 0;
    }
  }

  return -1;
}

const char *dt_node_props(struct device_node *node, const char *name) {
  struct property *p = node->prop;

  for(; p; p = p->next) {
    if(strcmp(p->name, name) == 0)
      return (const char *)p->data;
  }

  return NULL;
}

bool dt_node_props_is(struct device_node *node, const char *name, const char *str) {
  const char *s = dt_node_props(node, name);
  if(!s)
    return false;
  
  return !strcmp(s, str);
}

bool dt_node_propb(struct device_node *node, const char *name) {
  struct property *p = node->prop;

  for(; p; p = p->next) {
    if(strcmp(p->name, name) == 0)
      return true;
  }

  return false;
}

static inline u64 prop_read_number(fdt32 *prop, int size) {
  u64 n = 0;

  while(size-- > 0) {
    n = (n << 32) | fdt32_to_u32(*prop++);
  }

  return n;
}

static int dt_bus_ncells(struct device_node *bus, u32 *addr_cells, u32 *size_cells) {
  u32 na, ns;
  int rc;

  rc = dt_node_propa(bus, "#address-cells", &na);
  if(rc < 0)
    return -1;

  rc = dt_node_propa(bus, "#size-cells", &ns);
  if(rc < 0)
    return -1;

  if(addr_cells)
    *addr_cells = na;
  if(size_cells)
    *size_cells = ns;

  return 0;
}

/* ranges: <child_addr parent_addr size> */
u64 dt_bus_addr_translate(struct device_node *bus_node, u64 child_addr, u64 *paddr) {
  u32 len, na, ns, npa, oneroom;
  u64 parent_addr;
  fdt32 *ranges = dt_node_prop_raw(bus_node, "ranges", &len);
  struct device_node *parent;
  int nentry;

  /* same address space */
  if(!ranges || len == 0) {
    parent_addr = child_addr;
    goto done;
  }

  if(dt_bus_ncells(bus_node, &na, &ns) < 0)
    return -1;

  parent = bus_node->parent;
  if(!parent)
    return -1;

  if(dt_bus_ncells(parent, &npa, NULL) < 0)
    return -1;

  oneroom = na + ns + npa;
  nentry = (len / 4) / oneroom;

  for(int i = 0; i < nentry; i++) {
    u64 c_start = prop_read_number(ranges + (i * oneroom), na);
    u64 p_start = prop_read_number(ranges + (i * oneroom) + na, npa);
    u64 tsize = prop_read_number(ranges + (i * oneroom) + na + npa, ns);

    if(c_start <= child_addr && child_addr < c_start + tsize) {
      parent_addr = p_start + (child_addr - c_start);
      goto done;
    }
  }

  earlycon_puts("cannot translate\n");
  return -1;

done:
  if(paddr)
    *paddr = parent_addr;

  return 0;
}

int dt_node_prop_addr(struct device_node *node, int index, u64 *addr, u64 *size) {
  struct device_node *bus = node->parent;
  fdt32 *regs;
  u32 na, ns, len;
  int i;

  if(dt_bus_ncells(bus, &na, &ns) < 0)
    return -1;

  regs = dt_node_prop_raw(node, "reg", &len);
  if(!regs)
    return -1;

  len /= 4;

  i = index * (na + ns);
  if(i + na + ns > len)
    return -1;

  if(addr) {
    u64 ad, a = prop_read_number(regs + i, na);
    if(dt_bus_addr_translate(bus, a, &ad) < 0)
      return -1;

    *addr = ad;
  }

  if(size)
    *size = prop_read_number(regs + i + na, ns);

  return 0;
}

bool dt_node_device_type_is(struct device_node *node, const char *type) {
  return !strcmp(node->device_type, type);
}

struct device_node *dt_find_node_type_cont(const char *type, struct device_node *cont) {
  for(struct device_node *n = next_node(cont); n; n = next_node(n)) {
    if(strcmp(n->device_type, type) == 0)
      return n;
  }

  return NULL;
}

static bool node_name_is(struct device_node *node, const char *name) {
  const char *at;

  at = strchr(node->name, '@');

  if(at) {
    u32 baselen = at - node->name;

    return !strncmp(node->name, name, baselen);
  } else {
    return !strcmp(node->name, name);
  }
}

struct device_node *dt_find_node_path(const char *path) {
  struct device_node *root = localnode.device_tree;

  if(strcmp(path, "/") == 0)
    return root;

  while(*path == '/')
    path++;

  for(struct device_node *child = root->child; child; child = child->next) {
    if(node_name_is(child, path))
      return child;
  }

  return NULL;
}

struct device_node *next_match_node(struct dt_device *table, struct dt_device **dev,
                                    struct device_node *prev) {
  struct dt_device *d;

  for(struct device_node *n = next_node(prev); n; n = next_node(n)) {
    if((d = dt_compatible_device(table, n)) != NULL) {
      if(dev)
        *dev = d;

      return n;
    }
  }

  return NULL;
}

struct dt_device *dt_compatible_device(struct dt_device *table, struct device_node *node) {
  const char *compat = dt_node_props(node, "compatible");

  if(!compat)
    return NULL;

  for(struct dt_device *dev = table; dev->dev_name || dev->compat; dev++) {
    for(struct dt_compatible *c = dev->compat; c->comp; c++) {
      if(strcmp(c->comp, compat) == 0)
        return dev;
    }
  }

  return NULL;
}

int compat_dt_device_init(struct dt_device *table, struct device_node *node,
                           const char *compat) {
  for(struct dt_device *dev = table; dev->dev_name[0] || dev->compat; dev++) {
    for(struct dt_compatible *c = dev->compat; c->comp; c++) {
      if(strcmp(c->comp, compat) == 0) {
        if(dev->init) {
          dev->init(node);
          return 0;
        }
      }
    }
  }

  return -1;
}

static void setup_system_memory() {
  u64 base, size;
  u32 len, nranges, na, ns, oneroom;
  fdt32 *memranges;

  struct device_node *memdev = dt_find_node_path("/memory");
  if(!memdev)
    panic("no memory device");

  if(!dt_node_device_type_is(memdev, "memory"))
    panic("memory device type");

  struct device_node *bus = memdev->parent;
  if(dt_bus_ncells(bus, &na, &ns) < 0)
    panic("memory err");
  
  memranges = dt_node_prop_raw(memdev, "reg", &len);
  if(!memranges)
    panic("memory err");

  oneroom = na + ns;
  nranges = len / 4 / (oneroom);

  for(int i = 0; i < nranges; i++) {
    u64 start = prop_read_number(memranges + (i * oneroom), na);
    u64 size = prop_read_number(memranges + (i * oneroom) + na, ns);

    system_memory_reg(start, size);
  }
}

struct device_node *dt_next_cpu_device(struct device_node *prev) {
  struct device_node *cpus, *start;

  if(!prev) {
    cpus = dt_find_node_path("/cpus");
  } else {
    cpus = prev->parent;
  }

  start = prev ? prev->next : cpus->child;

  for(struct device_node *child = start; child; child = child->next) {
    if(dt_node_device_type_is(child, "cpu")) {
      return child;
    }
  }

  return NULL;
}

void device_tree_init(void *fdt_base) {
  struct fdt fdt;

  fdt_probe(&fdt, fdt_base);

  struct device_node *root = fdt_parse(&fdt);
  localnode.device_tree = root;

  const char *mach = dt_node_props(root, "compatible");
  if(mach)
    earlycon_puts(mach);

  setup_system_memory();
}
