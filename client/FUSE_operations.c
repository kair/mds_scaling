
#include "common/cache.h"
#include "common/connection.h"
#include "common/debugging.h"
#include "common/defaults.h"
#include "common/rpc_giga.h"

#include "backends/operations.h"

#include "client.h"
#include "FUSE_operations.h"

#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fuse.h>
#include <rpc/rpc.h>
#include <unistd.h>

static int  parse_path_components(const char *path, char *file, char *dir);
static void get_full_path(char fpath[MAX_LEN], const char *path);

#define FUSE_ERROR(x)   -(x)

/*
 * Parse the full path name of the file to give the name of the file and the
 * name of the file's parent directory.
 *
 * -- Example: A path "/a/b/f.txt" yields "f.txt" as file and "b" as dir
 */
static int parse_path_components(const char *path, char *file, char *dir)
{
	const char *p = path;
	if (!p || !file)
		return -1;

	if (strcmp(path, "/") == 0) {
		strcpy(file, "/");
		if (dir)
			strcpy(dir, "/");

		return 0;
	}

    int pathlen = strlen(path);

    if (pathlen > MAX_LEN)
        return -ENAMETOOLONG;
    
    p += pathlen;
	while ( (*p) != '/' && p != path)
		p--; // Come back till '/'

    if (pathlen - (int)(p - path) > MAX_LEN)
        return -ENAMETOOLONG;

    // Copy after slash till end into filename
	strncpy(file, p+1, MAX_LEN); 
	if (dir) {
		if (path == p)
			strncpy(dir, "/", 2);
		else {
            // Copy rest into dirpath 
			strncpy(dir, path, (int)(p - path )); 
			dir[(int)(p - path)] = '\0';
		}
	}
    
    logMessage(LOG_TRACE, __func__,
               "parsed [%s] to {file=[%s],dir=[%s]}", path, file, dir);
	return 0;
}

/*
 * Appends a given path name with something else.
 *
 */
static void get_full_path(char fpath[MAX_LEN], const char *path)
{
    strncpy(fpath, 
            giga_options_t.mountpoint, strlen(giga_options_t.mountpoint)+1);  
    strncat(fpath, path, MAX_LEN);          //XXX: long path names with break!

    logMessage(LOG_DEBUG, __func__, "converted [%s] to [%s]", path, fpath);

    return;
}

void* GIGAinit(struct fuse_conn_info *conn)
{
    logMessage(LOG_TRACE, __func__, " ==> init() ");

    (void)conn;

    switch (giga_options_t.backend_type) {
        case BACKEND_RPC_LOCALFS:
            ;
        case BACKEND_RPC_LEVELDB:
            if (rpcConnect() < 0) {
                logMessage(LOG_FATAL, __func__, "RPC_conn_err:%s", strerror(errno));
                exit(1);
            }
            
            if (rpc_init() < 0) {
                logMessage(LOG_TRACE, __func__, "RPC_init_err!!!");
                exit(1);
            }  
            
            break;
        
        default:
            break;
    }
    
    return NULL;
}

void GIGAdestroy(void * unused)
{
    (void)unused;

    logClose();

    /* FIXME: check cleanup code.
    rpc_disconnect();
    */
}


int GIGAgetattr(const char *path, struct stat *statbuf)
{
    logMessage(LOG_TRACE, __func__,
               " ==> getattr(path=[%s], statbuf=0x%08x) ", path, statbuf);

    int ret = 0;
    char fpath[MAX_LEN] = {0};
    char dir[MAX_LEN] = {0};
    char file[MAX_LEN] = {0};
    int dir_id = 0;
   
    switch (giga_options_t.backend_type) {
        case BACKEND_LOCAL_FS: 
            get_full_path(fpath, path);
            ret = local_getattr(fpath, statbuf);
            ret = FUSE_ERROR(ret);
            break;
        case BACKEND_RPC_LEVELDB:
            parse_path_components(path, dir, file);
            //TODO: convert "dir" to "dir_id"
            ret = rpc_getattr(dir_id, file, statbuf);
            ret = FUSE_ERROR(ret);
            break;
        default:
            break;
    }

    return ret;
}

int GIGAmkdir(const char *path, mode_t mode)
{
    logMessage(LOG_TRACE, __func__, 
               " ==> mkdir(path=[%s], mode=[0%3o])", path, mode);

    int ret = 0;
    char fpath[MAX_LEN] = {0};
    char dir[MAX_LEN] = {0};
    char file[MAX_LEN] = {0};
    int dir_id = 0;

    switch (giga_options_t.backend_type) {
        case BACKEND_LOCAL_FS:
            get_full_path(fpath, path);
            ret = local_mkdir(fpath, mode);
            ret = FUSE_ERROR(ret);
            break;
        case BACKEND_RPC_LEVELDB:
            parse_path_components(path, dir, file);
            ret = rpc_mkdir(dir_id, path, mode);
            ret = FUSE_ERROR(ret);
            break;
        default:
            break;
    }

    return ret;
}

/*
#######
*/

    
int GIGAmknod(const char *path, mode_t mode, dev_t dev)
{
    logMessage(LOG_TRACE, __func__, 
               " ==> mknod(path=[%s],mode=[0%3o],dev=[%lld])", path, mode, dev);

    int ret = 0;
    char fpath[PATH_MAX];
    
    switch (giga_options_t.backend_type) {
        case BACKEND_LOCAL_FS:
            get_full_path(fpath, path);
            ret = local_mknod(fpath, mode, dev);
            ret = FUSE_ERROR(ret);
            break;
        case BACKEND_RPC_LEVELDB:
            break;
        default:
            break;
    }

    return ret;
}

int GIGAsymlink(const char *path, const char *link)
{
    logMessage(LOG_TRACE, __func__,
               " ==> symlink(path=[%s], link=[%s])", path, link);

    int ret = 0;
    char flink[MAX_LEN] = {0};

    switch (giga_options_t.backend_type) {
        case BACKEND_LOCAL_FS:
            get_full_path(flink, link);
            ret = local_symlink(path, flink);
            ret = FUSE_ERROR(ret);
            break;
        case BACKEND_RPC_LEVELDB:
            break;
        default:
            break;
    }
    
    return ret;
}


int GIGAreadlink(const char *path, char *link, size_t size)
{
    logMessage(LOG_TRACE, __func__,
               " ==> readlink(path=[%s],link=[%s],size[%d])", path, link, size);
    
    int ret = 0;
    char fpath[MAX_LEN] = {0};

    switch (giga_options_t.backend_type) {
        case BACKEND_LOCAL_FS:
            get_full_path(fpath, path);
            ret = local_readlink(fpath, link, size);
            ret = FUSE_ERROR(ret);
            break;
        case BACKEND_RPC_LEVELDB:
            break;
        default:
            break;
    }
    
    logMessage(LOG_TRACE, __func__,
               "ret_readlink(link=[%s], size[%d])", path, link, strlen(link));
    
    return ret;
}

int GIGAopen(const char *path, struct fuse_file_info *fi)
{
    logMessage(LOG_TRACE, __func__, 
               " ==> open(path=[%s], fi=[0x%08x])", path, fi);
    
    int ret = 0;
    char fpath[MAX_LEN] = {0};
    int fd;

    switch (giga_options_t.backend_type) {
        case BACKEND_LOCAL_FS:
            get_full_path(fpath, path);
            if ((ret = local_open(fpath, fi->flags, &fd)) < 0)
                ret = FUSE_ERROR(ret);
            fi->fh = fd;
            break;
        case BACKEND_RPC_LEVELDB:
            break;
        default:
            break;
    }

    /*
    if ((fd = open(fpath, fi->flags)) < 0) {
        logMessage(LOG_FATAL, __func__,
                   "open(%s) failed: %s", fpath, strerror(errno));
        ret = FUSE_ERROR(errno);
    }
    
    fi->fh = fd;
    */

    logMessage(LOG_TRACE, __func__, 
               " ret_open(path=[%s], fi=[%d])", path, fi->fh);
    
    return ret;
}
    
