#include <buffer/cc_dbuf.h>

#include <cc_bstring.h>
#include <cc_debug.h>
#include <cc_mm.h>

#include <stddef.h>

#define DBUF_MODULE_NAME "ccommon::buffer/dbuf"

static bool dbuf_init = false;

/* Default buffer size, also the minimum size of the buffer */
static uint32_t dbuf_init_size = DBUF_DEFAULT_SIZE;

/* Maximum size of the buffer */
static uint32_t dbuf_max_size = DBUF_DEFAULT_MAX_SIZE;

/* When the buffer's size is >= dbuf_shrink_factor * unread bytes, it will shrink */
static uint32_t dbuf_shrink_factor = DBUF_DEFAULT_SHRINK;

void
dbuf_setup(uint32_t init_size, uint32_t max_size)
{
    log_info("set up the %s module", DBUF_MODULE_NAME);

    dbuf_init_size = init_size;
    dbuf_max_size = max_size;

    if(dbuf_init) {
        log_warn("%s has already been setup, overwrite", DBUF_MODULE_NAME);
    }

    dbuf_init = true;

    log_info("buffer/dbuf: init size %zu max size %zu", dbuf_init_size, dbuf_max_size);
}

void
dbuf_teardown(void)
{
    log_info("tear down the %s module", DBUF_MODULE_NAME);

    if(!dbuf_init) {
        log_warn("%s was not setup", DBUF_MODULE_NAME);
    }

    dbuf_init = false;
}

struct dbuf *
dbuf_create(void)
{
    struct dbuf *buf;

    if((buf = cc_alloc(sizeof(struct dbuf))) == NULL) {
        return NULL;
    }

    if((buf->begin = cc_alloc(dbuf_init_size)) == NULL) {
        cc_free(buf);
        return NULL;
    }

    buf->end = buf->begin + dbuf_init_size;
    buf->rpos = buf->wpos = buf->begin;

    return buf;
}

void
dbuf_destroy(struct dbuf *buf)
{
    cc_free(buf->begin);
    cc_free(buf);
}

/* Get number of unread bytes */
static inline uint32_t
dbuf_unread_size(struct dbuf *buf)
{
    return buf->wpos - buf->rpos;
}

void
dbuf_shift(struct dbuf *buf)
{
    if(buf->rpos == buf->wpos) {
        buf->rpos = buf->wpos = buf->begin;
    } else {
        cc_memmove(buf->begin, buf->rpos, dbuf_unread_size(buf));
        buf->wpos = buf->begin + dbuf_unread_size(buf);
        buf->rpos = buf->begin;
    }
}

static inline uint32_t
dbuf_read_capacity(struct dbuf *buf)
{
    ASSERT(buf->rpos <= buf->wpos);

    return buf->wpos - buf->rpos;
}

static inline uint32_t
dbuf_write_capacity(struct dbuf *buf)
{
    ASSERT(buf->wpos <= buf->end);

    return buf->end - buf->wpos;
}

/* The total capacity for this buffer */
static inline uint32_t
dbuf_capacity(struct dbuf *buf)
{
    return buf->end - buf->begin;
}

static rstatus_t
dbuf_double(struct dbuf *buf)
{
    ASSERT(buf->rpos == buf->begin);
    ASSERT(dbuf_capacity(buf) <= dbuf_max_size);

    uint32_t new_capacity = dbuf_capacity(buf) * 2;

    if(new_capacity > dbuf_max_size) {
        return CC_ERROR;
    }

    buf->begin = cc_realloc(buf->begin, new_capacity);

    if(buf->begin == NULL) {
        return CC_ENOMEM;
    }

    buf->end = buf->begin + new_capacity;

    return CC_OK;
}

rstatus_t
dbuf_fit(struct dbuf *buf, uint32_t count)
{
    uint32_t new_size;

    /* Calculate new_size as the closest 2 KiB that can hold count bytes plus
       what is already in buf */
    new_size = (dbuf_unread_size(buf) + count) % (2 * KiB) == 0 ? 2 * KiB : 0;
    new_size += ((dbuf_unread_size(buf) + count) / (2 * KiB)) * 2 * KiB;

    new_size = new_size >= dbuf_init_size ? new_size : dbuf_init_size;

    return dbuf_resize(buf, new_size);
}

rstatus_t
dbuf_resize(struct dbuf *buf, uint32_t new_size)
{
    if(new_size > dbuf_max_size || new_size < dbuf_unread_size(buf) ||
       new_size < dbuf_init_size) {
        /* new_size is invalid */
        return CC_ERROR;
    }

    buf->begin = cc_realloc(buf->begin, new_size);

    if(buf->begin == NULL) {
        return CC_ENOMEM;
    }

    buf->end = buf->begin + new_size;

    return CC_OK;
}

uint32_t
dbuf_read(uint8_t *dst, uint32_t count, struct dbuf *buf)
{
    uint32_t read_len = dbuf_read_capacity(buf) < count
        ? dbuf_read_capacity(buf) : count;

    cc_memcpy(dst, buf->rpos, read_len);
    buf->rpos += read_len;

    if(dbuf_capacity(buf) > dbuf_init_size &&
       dbuf_capacity(buf) > dbuf_unread_size(buf) * dbuf_shrink_factor) {
        /* Shrink buffer */
        dbuf_shift(buf);
        dbuf_fit(buf, 0);
    }

    return read_len;
}

rstatus_t
dbuf_write(uint8_t *src, uint32_t count, struct dbuf *buf)
{
    rstatus_t ret = CC_OK;

    if(dbuf_write_capacity(buf) < count) {
        /* buf needs to be resized */
        if(dbuf_capacity(buf) * 2 < dbuf_unread_size(buf) + count) {
            /* Doubling the buffer still won't be able to contain count bytes */
            dbuf_shift(buf);
            ret = dbuf_fit(buf, count);
        } else {
            dbuf_shift(buf);

            if(dbuf_write_capacity(buf) < count) {
                /* Double the buffer */
                ret = dbuf_double(buf);

                if(ret != CC_OK) {
                    /* Doubling results in oversized buffer */
                    dbuf_shift(buf);
                    ret = dbuf_fit(buf, count);
                }
            }
        }
    }

    if(ret != CC_OK) {
        return ret;
    }

    ASSERT(dbuf_write_capacity(buf) >= count);

    cc_memcpy(buf->wpos, src, count);
    buf->wpos += count;

    return CC_OK;
}
