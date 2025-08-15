#include "sqlite3ext.h"
SQLITE_EXTENSION_INIT1

#include <assert.h>
#include <string.h>
#include <stdlib.h>

// 在每个数据库主文件的开头要跳过的头部大小
#define HEADER_SIZE 1024

/*
** VFS 的 sqlite3_file 对象
*/
typedef struct HeaderFile {
    sqlite3_file base; /* 基类。必须是第一个成员。 */
    sqlite3_file *pRealFile; /* 来自底层 VFS 的真实文件句柄 */
} HeaderFile;


/****************************************************************************
** I/O 方法实现
****************************************************************************/


static int headerClose(sqlite3_file *pFile) {
    HeaderFile *p = (HeaderFile *) pFile;
    int rc = p->pRealFile->pMethods->xClose(p->pRealFile);
    sqlite3_free(p->pRealFile);
    return rc;
}

/*
** 从文件中读取数据。
** 读取操作在 iOfst + HEADER_SIZE 的偏移量处执行。
*/
static int headerRead(
    sqlite3_file *pFile,
    void *zBuf,
    int iAmt,
    sqlite3_int64 iOfst
) {
    HeaderFile *p = (HeaderFile *) pFile;
    return p->pRealFile->pMethods->xRead(p->pRealFile, zBuf, iAmt, iOfst + HEADER_SIZE);
}

/*
** 向文件中写入数据。
** 写入操作在 iOfst + HEADER_SIZE 的偏移量处执行。
*/
static int headerWrite(
    sqlite3_file *pFile,
    const void *zBuf,
    int iAmt,
    sqlite3_int64 iOfst
) {
    HeaderFile *p = (HeaderFile *) pFile;
    return p->pRealFile->pMethods->xWrite(p->pRealFile, zBuf, iAmt, iOfst + HEADER_SIZE);
}

/*
** 截断文件。
** 截断操作在 size + HEADER_SIZE 的大小处执行。
*/
static int headerTruncate(sqlite3_file *pFile, sqlite_int64 size) {
    HeaderFile *p = (HeaderFile *) pFile;
    return p->pRealFile->pMethods->xTruncate(p->pRealFile, size + HEADER_SIZE);
}

/*
** 同步文件。
** 这是一个到底层 VFS 的简单传递。
*/
static int headerSync(sqlite3_file *pFile, int flags) {
    HeaderFile *p = (HeaderFile *) pFile;
    return p->pRealFile->pMethods->xSync(p->pRealFile, flags);
}

/*
** 获取文件大小。
** 报告的大小是物理大小减去头部大小。
*/
static int headerFileSize(sqlite3_file *pFile, sqlite_int64 *pSize) {
    HeaderFile *p = (HeaderFile *) pFile;
    sqlite3_int64 realSize;
    int rc = p->pRealFile->pMethods->xFileSize(p->pRealFile, &realSize);
    if (rc == SQLITE_OK) {
        *pSize = (realSize > HEADER_SIZE) ? (realSize - HEADER_SIZE) : 0;
    }
    return rc;
}

/*
** 以下都是简单的传递方法。
*/
static int headerLock(sqlite3_file *pFile, int eLock) {
    HeaderFile *p = (HeaderFile *) pFile;
    return p->pRealFile->pMethods->xLock(p->pRealFile, eLock);
}

static int headerUnlock(sqlite3_file *pFile, int eLock) {
    HeaderFile *p = (HeaderFile *) pFile;
    return p->pRealFile->pMethods->xUnlock(p->pRealFile, eLock);
}

static int headerCheckReservedLock(sqlite3_file *pFile, int *pResOut) {
    HeaderFile *p = (HeaderFile *) pFile;
    return p->pRealFile->pMethods->xCheckReservedLock(p->pRealFile, pResOut);
}

static int headerFileControl(sqlite3_file *pFile, int op, void *pArg) {
    HeaderFile *p = (HeaderFile *) pFile;
    return p->pRealFile->pMethods->xFileControl(p->pRealFile, op, pArg);
}

static int headerSectorSize(sqlite3_file *pFile) {
    HeaderFile *p = (HeaderFile *) pFile;
    return p->pRealFile->pMethods->xSectorSize(p->pRealFile);
}

static int headerDeviceCharacteristics(sqlite3_file *pFile) {
    HeaderFile *p = (HeaderFile *) pFile;
    return p->pRealFile->pMethods->xDeviceCharacteristics(p->pRealFile);
}

/*
** 以下是为 WAL 支持的传递方法。
*/
static int headerShmMap(
    sqlite3_file *pFile,
    int iPg,
    int pgsz,
    int bExtend,
    void volatile **pp
) {
    HeaderFile *p = (HeaderFile *) pFile;
    return p->pRealFile->pMethods->xShmMap(p->pRealFile, iPg, pgsz, bExtend, pp);
}

static int headerShmLock(sqlite3_file *pFile, int offset, int n, int flags) {
    HeaderFile *p = (HeaderFile *) pFile;
    return p->pRealFile->pMethods->xShmLock(p->pRealFile, offset, n, flags);
}

static void headerShmBarrier(sqlite3_file *pFile) {
    HeaderFile *p = (HeaderFile *) pFile;
    p->pRealFile->pMethods->xShmBarrier(p->pRealFile);
}

static int headerShmUnmap(sqlite3_file *pFile, int deleteFlag) {
    HeaderFile *p = (HeaderFile *) pFile;
    return p->pRealFile->pMethods->xShmUnmap(p->pRealFile, deleteFlag);
}


/****************************************************************************
** VFS 方法实现
****************************************************************************/

/*
** 打开文件。
*/
static int headerOpen(
    sqlite3_vfs *pVfs, /* VFS */
    const char *zName, /* 要打开的文件 */
    sqlite3_file *pFile, /* 指向要填充的 HeaderFile 结构的指针 */
    int flags, /* 输入的 SQLITE_OPEN_XXX 标志 */
    int *pOutFlags /* 输出的 SQLITE_OPEN_XXX 标志 */
) {
    /* VFS 的自定义 I/O 方法。它包含 WAL 支持。 */
    static const sqlite3_io_methods header_io_methods = {
        3, /* iVersion 3， SQLite 3.7.6（2011-04-12）开始支持 */
        headerClose,
        headerRead,
        headerWrite,
        headerTruncate,
        headerSync,
        headerFileSize,
        headerLock,
        headerUnlock,
        headerCheckReservedLock,
        headerFileControl,
        headerSectorSize,
        headerDeviceCharacteristics,
        /* V2/V3 WAL 支持方法 */
        headerShmMap,
        headerShmLock,
        headerShmBarrier,
        headerShmUnmap,
        0, /* xFetch */
        0 /* xUnfetch */
    };

    int rc;
    HeaderFile *p = (HeaderFile *) pFile;
    sqlite3_vfs *pRealVfs = (sqlite3_vfs *) pVfs->pAppData;

    p->pRealFile = sqlite3_malloc(pRealVfs->szOsFile);
    if (!p->pRealFile) {
        return SQLITE_NOMEM;
    }
    memset(p->pRealFile, 0, pRealVfs->szOsFile);

    /* 使用底层 VFS 打开文件 */
    rc = pRealVfs->xOpen(pRealVfs, zName, p->pRealFile, flags, pOutFlags);

    if (rc == SQLITE_OK) {
        /* 关键改动：区分主数据库文件和其他文件。
        ** 头部偏移逻辑只应应用于主数据库文件。
        ** 日志、WAL 和临时文件应被透明处理。
        */
        if (flags & SQLITE_OPEN_MAIN_DB) {
            p->base.pMethods = &header_io_methods;
            if (flags & SQLITE_OPEN_CREATE) {
                sqlite3_int64 currentSize;
                if (p->pRealFile->pMethods->xFileSize(p->pRealFile, &currentSize) == SQLITE_OK
                    && currentSize == 0
                ) {
                    rc = p->pRealFile->pMethods->xTruncate(p->pRealFile, HEADER_SIZE);
                }
            }
        } else {
            p->base.pMethods = p->pRealFile->pMethods;
        }
    }

    if (rc != SQLITE_OK) {
        if (p->pRealFile->pMethods) {
            p->pRealFile->pMethods->xClose(p->pRealFile);
        }
        sqlite3_free(p->pRealFile);
        p->pRealFile = 0;
    }

    return rc;
}

/*
** 以下 VFS 方法是到底层 VFS 的简单传递。
** pVfs->pAppData 字段保存着指向真实 VFS 的指针。
*/
static int headerDelete(sqlite3_vfs *pVfs, const char *zPath, int dirSync) {
    sqlite3_vfs *pRealVfs = (sqlite3_vfs *) pVfs->pAppData;
    return pRealVfs->xDelete(pRealVfs, zPath, dirSync);
}

static int headerAccess(sqlite3_vfs *pVfs, const char *zPath, int flags, int *pResOut) {
    sqlite3_vfs *pRealVfs = (sqlite3_vfs *) pVfs->pAppData;
    return pRealVfs->xAccess(pRealVfs, zPath, flags, pResOut);
}

static int headerFullPathname(sqlite3_vfs *pVfs, const char *zPath, int nPathOut, char *zPathOut) {
    sqlite3_vfs *pRealVfs = (sqlite3_vfs *) pVfs->pAppData;
    return pRealVfs->xFullPathname(pRealVfs, zPath, nPathOut, zPathOut);
}

static void *headerDlOpen(sqlite3_vfs *pVfs, const char *zPath) {
    sqlite3_vfs *pRealVfs = (sqlite3_vfs *) pVfs->pAppData;
    return pRealVfs->xDlOpen(pRealVfs, zPath);
}

static void headerDlError(sqlite3_vfs *pVfs, int nByte, char *zErrMsg) {
    sqlite3_vfs *pRealVfs = (sqlite3_vfs *) pVfs->pAppData;
    pRealVfs->xDlError(pRealVfs, nByte, zErrMsg);
}

static void (*headerDlSym(sqlite3_vfs *pVfs, void *pH, const char *zSym))(void) {
    sqlite3_vfs *pRealVfs = (sqlite3_vfs *) pVfs->pAppData;
    return pRealVfs->xDlSym(pRealVfs, pH, zSym);
}

static void headerDlClose(sqlite3_vfs *pVfs, void *pHandle) {
    sqlite3_vfs *pRealVfs = (sqlite3_vfs *) pVfs->pAppData;
    pRealVfs->xDlClose(pRealVfs, pHandle);
}

static int headerRandomness(sqlite3_vfs *pVfs, int nByte, char *zByte) {
    sqlite3_vfs *pRealVfs = (sqlite3_vfs *) pVfs->pAppData;
    return pRealVfs->xRandomness(pRealVfs, nByte, zByte);
}

static int headerSleep(sqlite3_vfs *pVfs, int nMicro) {
    sqlite3_vfs *pRealVfs = (sqlite3_vfs *) pVfs->pAppData;
    return pRealVfs->xSleep(pRealVfs, nMicro);
}

static int headerCurrentTime(sqlite3_vfs *pVfs, double *pTime) {
    sqlite3_vfs *pRealVfs = (sqlite3_vfs *) pVfs->pAppData;
    return pRealVfs->xCurrentTime(pRealVfs, pTime);
}

static int headerGetLastError(sqlite3_vfs *pVfs, int n, char *s) {
    sqlite3_vfs *pRealVfs = (sqlite3_vfs *) pVfs->pAppData;
    return pRealVfs->xGetLastError(pRealVfs, n, s);
}

static int headerCurrentTimeInt64(sqlite3_vfs *pVfs, sqlite3_int64 *pTime) {
    sqlite3_vfs *pRealVfs = (sqlite3_vfs *) pVfs->pAppData;
    return pRealVfs->xCurrentTimeInt64(pRealVfs, pTime);
}

/****************************************************************************
** 扩展注册函数
****************************************************************************/


// 全局静态的 VFS 结构体实例
static sqlite3_vfs header_vfs;

/*
** SQLite 扩展的入口点函数。
** 当执行 `SELECT load_extension(...)` 时，SQLite 会调用此函数。
*/
#ifdef _WIN32
__declspec(dllexport)
#endif
int sqlite3_headervfs_init(
    sqlite3 *db,
    char **pzErrMsg,
    const sqlite3_api_routines *pApi
) {
    int rc = SQLITE_OK;
    sqlite3_vfs *pDefaultVfs;

    SQLITE_EXTENSION_INIT2(pApi);

    pDefaultVfs = sqlite3_vfs_find(0);
    if (pDefaultVfs == 0) {
        return SQLITE_ERROR;
    }

    /* 复制默认 VFS 的内容到我们的结构体中，以继承其方法 */
    memcpy(&header_vfs, pDefaultVfs, sizeof(sqlite3_vfs));

    /* 设置 VFS 的名字 */
    header_vfs.zName = "headervfs";

    /* 存储指向真实 VFS 的指针，以便后续的传递调用 */
    header_vfs.pAppData = pDefaultVfs;

    /* 设置自定义的 sqlite3_file 结构的大小 */
    header_vfs.szOsFile = sizeof(HeaderFile);

    /* 覆写我们需要修改的 VFS 方法 */
    header_vfs.xOpen = headerOpen;
    header_vfs.xDelete = headerDelete;
    header_vfs.xAccess = headerAccess;
    header_vfs.xFullPathname = headerFullPathname;
    header_vfs.xDlOpen = headerDlOpen;
    header_vfs.xDlError = headerDlError;
    header_vfs.xDlSym = headerDlSym;
    header_vfs.xDlClose = headerDlClose;
    header_vfs.xRandomness = headerRandomness;
    header_vfs.xSleep = headerSleep;
    header_vfs.xCurrentTime = headerCurrentTime;
    header_vfs.xGetLastError = headerGetLastError;
    header_vfs.xCurrentTimeInt64 = headerCurrentTimeInt64;

    /*
    ** 注册我们的 VFS。
    ** 第二个参数是 makeDefault，我们将其设置为 0 (false)。
    ** 用户需要通过 sqlite3_open_v2() 的第四个参数显式选择使用此 VFS。
    */
    rc = sqlite3_vfs_register(&header_vfs, 0);

    if (rc == SQLITE_OK) {
        rc = SQLITE_OK_LOAD_PERMANENTLY;
    }

    return rc;
}
