#include "postgres.h"
#include "funcapi.h"
#include <dirent.h>
#include <sys/stat.h>

typedef struct
{
    struct dirent **dir_ctx_entries;
    char *dir_ctx_name;
} dir_ctx;

static bool getFileInfo(struct stat *buf, char *dirName, char *fileName)
{
    char *pathName = (char *)palloc(strlen(dirName) + 1 + strlen(fileName) + 1);

    strcpy(pathName, dirName);
    strcat(pathName, "/");
    strcat(pathName, fileName);

    if (stat(pathName, buf) == 0)
        return (true);
    else
        return (false);
}
static char *text2cstring(text *src)
{
    int len = VARSIZE(src) - VARHDRSZ;
    char *dst = (char *)palloc(len + 1);

    memcpy(dst, src->vl_dat, len);
    dst[len] = '\0';

    return (dst);
}

PG_MODULE_MAGIC;

PG_FUNCTION_INFO_V1(fileinfo);

Datum fileinfo(PG_FUNCTION_ARGS)
{
    char *start = text2cstring(PG_GETARG_TEXT_P(0));
    dir_ctx *ctx;
    FuncCallContext *srf;

    if (SRF_IS_FIRSTCALL())
    {
        TupleDesc tupdesc;
        MemoryContext oldContext;

        srf = SRF_FIRSTCALL_INIT();

        oldContext = MemoryContextSwitchTo(srf->multi_call_memory_ctx);
        ctx = (dir_ctx *)palloc(sizeof(dir_ctx));

        tupdesc = RelationNameGetTupleDesc("fileinfo");

        srf->user_fctx = ctx;
        srf->max_calls = scandir(start, &ctx->dir_ctx_entries, NULL, alphasort);
        srf->attinmeta = TupleDescGetAttInMetadata(tupdesc);

        ctx->dir_ctx_name = start;

        MemoryContextSwitchTo(oldContext);
    }

    srf = SRF_PERCALL_SETUP();
    ctx = (dir_ctx *)srf->user_fctx;

    if (srf->max_calls == -1)
        SRF_RETURN_DONE(srf);

    if (srf->call_cntr < srf->max_calls)
    {
        struct dirent *entry;
        char *values[2];
        struct stat statBuf;
        char fileSizeStr[10 + 1] = {0};

        HeapTuple tuple;

        entry = ctx->dir_ctx_entries[srf->call_cntr];
        values[0] = entry->d_name;

        if (getFileInfo(&statBuf, ctx->dir_ctx_name, entry->d_name))
        {

            snprintf(fileSizeStr, sizeof(fileSizeStr), "%ld", statBuf.st_size);

            values[1] = fileSizeStr;
        }
        else
        {
            values[1] = NULL;
        }

        tuple = BuildTupleFromCStrings(srf->attinmeta, values);

        SRF_RETURN_NEXT(srf, HeapTupleGetDatum(tuple));
    }
    else
    {
        SRF_RETURN_DONE(srf);
    }
}
