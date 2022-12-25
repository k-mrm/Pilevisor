#ifndef VSM_LOG_H
#define VSM_LOG_H

#include "types.h"

#define VLOG_LEVEL  0

enum {
  READ_SENDER,
  READ_RECEIVER,
  WRITE_SENDER,
  WRITE_RECEIVER,
  INV_SENDER,
  INV_RECEIVER,
};

struct vsmlog {
  int type;     // read or write or invalidate
  int cpu;
  u64 ipa;
  const char *msg;
  int from_node;
  int to_node;
};

void vsm_logdump(int n);
void vsm_logging(int type, int from_node, int to_node, u64 ipa, const char *msg);
void vsm_log_out(int type, int from_node, int to_node, u64 ipa, const char *msg);

#if VLOG_LEVEL == 0

#define vsm_log(...)  ((void)0)

#elif VLOG_LEVEL == 1

#define vsm_log       vsm_logging

#elif VLOG_LEVEL == 2

#define vsm_log       vsm_log_out

#endif  /* VLOG_LEVEL */

#endif  /* VSM_LOG_H */
