#include <buffer/cc_dbuf.h>

#include <cc_bstring.h>
#include <cc_debug.h>
#include <cc_mm.h>

#include <stddef.h>

#define DBUF_MODULE_NAME "ccommon::buffer/dbuf"

static bool dbuf_init = false;

/* Maximum size of the buffer */
static uint32_t dbuf_max_size = DBUF_DEFAULT_MAX_SIZE;

/* When the buffer's size is >= dbuf_shrink_factor * unread bytes, it will shrink */
static uint32_t dbuf_shrink_factor = DBUF_DEFAULT_SHRINK;

void
dbuf_setup(uint32_t max_size, uint32_t shrink_factor)
{
    log_info("set up the %s module", DBUF_MODULE_NAME);

    dbuf_max_size = max_size;
    dbuf_shrink_factor = shrink_factor;

    if (dbuf_init) {
        log_warn("%s has already been setup, overwrite", DBUF_MODULE_NAME);
    }

    dbuf_init = true;

    log_info("buffer/dbuf: max size %zu", dbuf_max_size);
}

void
dbuf_teardown(void)
{
    log_info("tear down the %s module", DBUF_MODULE_NAME);

    if (!dbuf_init) {
        log_warn("%s was not setup", DBUF_MODULE_NAME);
    }

    dbuf_init = false;
}

void
dbuf_return(struct buf **buf)
{
    (*buf)->rpos = (*buf)->wpos = (*buf)->begin;
    dbuf_resize(*buf, buf_size);

    buf_return(buf);
}

rstatus_t
dbuf_double(struct buf *buf)
{
    ASSERT(buf_capacity(buf) <= dbuf_max_size);

    uint32_t new_capacity = buf_capacity(buf) * 2;

    if (new_capacity > dbuf_max_size) {
        return CC_ERROR;
    }

    buf = cc_realloc(buf, new_capacity);

    if (buf == NULL) {
        return CC_ENOMEM;
    }

    buf->end = (uint8_t *)buf + new_capacity;

    return CC_OK;
}

rstatus_t
dbuf_fit(struct buf *buf, uint32_t count)
{
    uint32_t new_size;

    /* Calculate new_size as the closest 2 KiB that can hold count bytes plus
       what is already in buf */
    new_size = ((buf_rsize(buf) + count) % (2 * KiB)) == 0 ? 2 * KiB : 0;
    new_size += ((buf_rsize(buf) + count) / (2 * KiB)) * 2 * KiB;

    new_size = new_size >= buf_size ? new_size : buf_size;

    return dbuf_resize(buf, new_size);
}

rstatus_t
dbuf_resize(struct buf *buf, uint32_t new_size)
{
    if (new_size > dbuf_max_size || new_size < buf_rsize(buf) ||
       new_size < buf_size) {
        /* new_size is invalid */
        return CC_ERROR;
    }

    buf = cc_realloc(buf, new_size);

    if (buf == NULL) {
        return CC_ENOMEM;
    }

    buf->end = (uint8_t *)buf + new_size;

    return CC_OK;
}

uint32_t
dbuf_read(uint8_t *dst, uint32_t count, struct buf *buf)
{
    uint32_t read_len = buf_read(dst, count, buf);

    if (buf_capacity(buf) > buf_size &&
       buf_capacity(buf) > buf_rsize(buf) * dbuf_shrink_factor) {
        /* Shrink buffer */
        buf_lshift(buf);
        dbuf_fit(buf, 0);
    }

    return read_len;
}

uint32_t
dbuf_write(uint8_t *src, uint32_t count, struct buf *buf)
{
    rstatus_t ret = CC_OK;

    if (buf_wsize(buf) < count) {
        buf_lshift(buf);
    }

    if (buf_wsize(buf) < count) {
        /* buf needs to be resized */
        if (buf_capacity(buf) * 2 < buf_rsize(buf) + count) {
            /* Doubling the buffer still won't be able to contain count bytes */
            ret = dbuf_fit(buf, count);
        } else {
            /* Double the buffer */
            ret = dbuf_double(buf);

            if (ret != CC_OK) {
                /* Doubling results in oversized buffer */
                ret = dbuf_fit(buf, count);
            }
        }
    }

    if (ret == CC_ENOMEM) {
        log_crit("Buffer expansion failed due to OOM");
        return 0;
    }

    if (ret != CC_OK) {
        log_warn("dbuf: write request size %zu too large to fit in max size dbuf", count);
    }

    return buf_write(src, count, buf);
}

uint32_t
dbuf_read_bstring(struct buf *buf, struct bstring *bstr)
{
    return dbuf_read(bstr->data, bstr->len, buf);
}

uint32_t
dbuf_write_bstring(struct buf *buf, const struct bstring *bstr)
{
    return dbuf_write(bstr->data, bstr->len, buf);
}
