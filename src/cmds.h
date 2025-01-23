// cmds.h: Declares subcommand functions
// The actual commands can be found in their respective .c files.

#ifndef __IASYNC_CMDS_H
#define __IASYNC_CMDS_H

typedef struct _global_args
{
    /// @brief Device name, or NULL if not provided
    char* name;
    /// @brief Device UDID, or NULL if not provided
    char* udid;
} globargs_t;

typedef int (*cmd_func_t)(int, char*[], globargs_t*);

#define __DECLARE_CMD(cn) int cmd_##cn(int, char*[], globargs_t*)

__DECLARE_CMD(lsdev);
__DECLARE_CMD(lsapps);
__DECLARE_CMD(ls);
__DECLARE_CMD(sync);

#endif /* !__IASYNC_CMDS_H */