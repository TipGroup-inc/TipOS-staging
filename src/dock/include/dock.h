#ifndef DOCK_H
#define DOCK_H

#define DOCK_VERSION "0.1.0"

typedef enum {
    DOCK_OK      = 0,
    DOCK_ERR_MEM = -1,
    DOCK_ERR_ABI = -2,
    DOCK_ERR_MANIFEST = -3,
    DOCK_ERR_LOAD = -4,
    DOCK_ERR_INIT = -5,
} dock_err_t;

#endif
