#include <memory.h>

/* enable logging & debugging */
#define CONFIG_ASSERT
#define CONFIG_LOG

#include "yldev.h"

#define _MAX_HBITS 32
#define _MIN_HBITS 4

#include "hash.h"
#include "yllist.h"
/* crc is used as hash function */
#include "crc.h"

/* hash node */
struct _hn {
    yllist_link_t         lk;
    unsigned char*        key;   /* full key */
    unsigned int          keysz; /* size of key */
    unsigned int          hv32;  /* 32bit hash value */
    void*                 v;     /* user value */
};

struct _sHash {
    yllist_link_t*        map;
    unsigned int          sz;          /* hash size */
    unsigned char         mapbits;     /* bits of map table = 1<<mapbits */
    void                (*fcb) (void*);/* free callback */
};

static inline unsigned int
_hmapsz (const hash_t* h) {
    return (1<<h->mapbits);
}

static inline unsigned int
_hv__ (unsigned int mapbits, unsigned int hv32) {
    return hv32 >> (32 - mapbits);
}

static inline unsigned int
_hv_ (const hash_t* h, unsigned int hv32) {
    return _hv__ (h->mapbits, hv32);
}

static inline unsigned int
_hv (const hash_t* h, const struct _hn* n) {
    return _hv_ (h, n->hv32);
}


/* Modify hash space to 'bits'*/
static hash_t*
_hmodify (hash_t* h, unsigned int bits) {
    int                 i;
    struct _hn         *n, *tmp;
    yllist_link_t*      oldmap;
    unsigned int        oldmapsz;

    if (bits < _MIN_HBITS) bits = _MIN_HBITS;
    if (bits > _MAX_HBITS) bits = _MAX_HBITS;

    if (h->mapbits == bits) return h; /* nothing to do */
    
    oldmap = h->map;
    oldmapsz = _hmapsz (h);

    h->mapbits = bits; /* map size is changed here */
    h->map = ylmalloc (sizeof (yllist_link_t) * _hmapsz (h));
    ylassert (h->map);
    for (i=0; i<_hmapsz (h); i++) yllist_init_link (&h->map[i]);
    /* re assign hash nodes */
    for (i=0; i<oldmapsz; i++) {
        yllist_foreach_item_removal_safe (n, tmp, &oldmap[i], struct _hn, lk) {
            yllist_del (&n->lk);
            yllist_add_last (&h->map[_hv(h, n)], &n->lk);
        }
    }
    ylfree (oldmap);
    return h;
}

static struct _hn*
_hfind (const hash_t* h, const unsigned char* key, unsigned int keysz) {
    struct _hn*     n;
    unsigned int    hv32 = crc32 (0, key, keysz);
    yllist_link_t*  hd = &h->map[_hv_ (h, hv32)];
    yllist_foreach_item (n, hd, struct _hn, lk) {
        if (keysz == n->keysz
            && n->hv32 == hv32
            && 0 == memcmp (key, n->key, keysz) ) break;
    }
    return (&n->lk == hd)? NULL: n;
}


static struct _hn*
_ncreate (const unsigned char* key, unsigned int keysz, void* v) {
    struct _hn* n = ylmalloc (sizeof (*n));
    ylassert (n);
    n->key = ylmalloc (keysz);
    ylassert (n->key);
    memcpy (n->key, key, keysz);
    n->keysz = keysz;
    n->hv32 = crc32 (0, key, keysz);
    n->v = v;
    yllist_init_link (&n->lk);
    return n;
}

static void
_ndestroy (const hash_t* h, struct _hn* n) {
    ylassert (n->key && n->keysz>0);
    ylfree (n->key);
    if (h->fcb) (*h->fcb) (n->v);
    ylfree (n);
}

hash_t*
hash_create (void(*fcb) (void*)) {
    int     i;
    hash_t* h = ylmalloc (sizeof (*h));
    ylassert (h);
    h->sz = 0;
    h->mapbits = _MIN_HBITS;
    h->map = (yllist_link_t*)ylmalloc (sizeof (yllist_link_t) * _hmapsz (h));
    ylassert (h->map);
    for (i=0; i<_hmapsz (h); i++) yllist_init_link (&h->map[i]);
    h->fcb = fcb;
    return h;
}

void
hash_destroy (hash_t* h) {
    int          i;
    struct _hn  *n, *tmp;
    for (i=0; i<_hmapsz (h); i++) {
        yllist_foreach_item_removal_safe (n, tmp, &h->map[i], struct _hn, lk) {
            yllist_del (&n->lk);
            _ndestroy (h, n);
        }
    }
    ylfree (h->map);
    ylfree (h);
}

unsigned int
hash_sz (hash_t* h) {
    return h->sz;
}

int
hash_add (hash_t* h,
            const unsigned char* key, unsigned int keysz,
            void* v) {
    int         r;
    struct _hn* n;
    /* delete it if key already exists */
    r = hash_del (h, key, keysz);

    /*
     * check return value from 'hash_del'.
     * 'successfully deleted' means 'key is going to be overwritten'
     */
    if (!r) r = 1; /* overwritten */
    else r = 0; /* newly added */

    /* we need to expand hash map size if hash seems to be full */
    if (h->sz > _hmapsz (h)) _hmodify (h, h->mapbits+1);

    n = _ncreate (key, keysz, v);
    yllist_add_last (&h->map[_hv (h, n)], &n->lk);
    h->sz++;

    return r;
}

int
hash_del (hash_t* h,
            const unsigned char* key, unsigned int keysz) {
    struct _hn* n = _hfind (h, key, keysz);
    if (n) {
        yllist_del (&n->lk);
        _ndestroy (h, n);
        h->sz--;
        if ( h->sz < _hmapsz (h)/4 ) _hmodify (h, h->mapbits-1);
        return 0;
    } else
        return -1;
}

void*
hash_get (hash_t* h,
            const unsigned char* key, unsigned int keysz) {
    struct _hn* n = _hfind (h, key, keysz);
    return n? n->v: NULL;
}

int
hash_walk (hash_t* h, void* user,
           /* return : b_keepgoing => return 1 for keep going, 0 for stop and don't do anymore*/
           int (cb) (void*/*user*/,
                     const unsigned char*/*key*/, unsigned int/*sz*/,
                     void*/*value*/)) {
    int          i;
    struct _hn*  n;
    ylassert (h && cb);
    for (i=0; i<_hmapsz (h); i++)
        yllist_foreach_item (n, &h->map[i], struct _hn, lk)
            if (!(*cb) (user, n->key, n->keysz, n->v)) return 0;

    return 1;
}
