#ifndef __HASh_h__
#define __HASh_h__

typedef struct _sHash hash_t;
/**
 * @fcb : callback to free user value(item)
 *        (NULL means, item doesn't need to be freed.)
 */
extern hash_t*
hash_create (void(*fcb) (void*));

extern void
hash_destroy (hash_t* h);

/**
 * @return : number of elements in hash.
 */
extern unsigned int
hash_sz (hash_t* h);

/**
 * @v      : user value(item)
 * @return:
 *   -1: error
 *    0: newly added
 *    1: overwritten
 */
extern int
hash_add (hash_t* h,
          const unsigned char* key, unsigned int keysz,
          void* v);

/**
 * If these is no matched item, nothing happened.
 * @v      : user value(item)
 * @return:
 *    <0: fail. cannot be found
 *    0 : deleted.
 */
extern int
hash_del (hash_t* h,
          const unsigned char* key, unsigned int keysz);

/**
 * @return : NULL if fails.
 */
extern void*
hash_get (hash_t* h,
          const unsigned char* key, unsigned int keysz);

/**
 * walking hash nodes
 * @return:
 *    1 : success. End of hash.
 *    0 : stop by user callback
 *   -1 : error.
 */
extern int
hash_walk (hash_t*, void* user,
           /* return : b_keepgoing => return 1 for keep going, 0 for stop and don't do anymore*/
           int (*cb) (void*/*user*/,
                      const unsigned char*/*key*/, unsigned int/*sz*/,
                      void*/*value*/));

#endif /* __HASh_h__ */
