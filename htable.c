/*
 *  Created on: 11 авг. 2016 г.
 *      Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */

#include "htable.h"
#include "../klib/crc.h"
#include "../klib/hash.h"

/*
 * Because crc16() return short:
 */
static inline unsigned int _crc16( const void *buf, size_t size )
{
    return crc16( buf, size );
}

static HT_Hash_Function _hf[] = { hash_faq6, hash_ly, hash_rot13, hash_rs, _crc16, crc32 };

HTable HT_create( HT_Hash_Functions hf, Tree_Flags flags,
                  Tree_Destroy destructor )
{
    HTable ht = NULL;

    if( hf >= HF_HASH_MAX ) {
        hf = 0;
    }

    ht = Malloc( sizeof( struct _HTable ) );

    if( ht ) {
        size_t i;

        for( i = 0; i <= UCHAR_MAX; i++ ) {
            /*
             * Always replace values (T_INSERT_REPLACE flag):
             */
            ht->bt[i] = AVL_create( flags | T_INSERT_REPLACE, destructor );

            if( !ht->bt[i] ) {
                while( i ) {
                    i--;
                    AVL_destroy( ht->bt[i] );
                }

                Free( ht );
                ht = NULL;
            }
        }

        __initlock( ht->lock );
        ht->hf = _hf[hf];
    }

    return ht;
}

void HT_clear( HTable ht )
{
    size_t i;
    __lock( ht->lock );

    for( i = 0; i <= UCHAR_MAX; i++ ) {
        AVL_clear( ht->bt[i] );
    }

    __unlock( ht->lock );
}

void HT_destroy( HTable ht )
{
    size_t i;
    __lock( ht->lock );

    for( i = 0; i <= UCHAR_MAX; i++ ) {
        AVL_destroy( ht->bt[i] );
    }

    Free( ht );
}

size_t HT_size( HTable ht )
{
    size_t rc = 0;
    size_t i = 0;
    __lock( ht->lock );

    while( i < UCHAR_MAX ) {
        rc += ht->bt[i]->nodes;
        i++;
    }

    __unlock( ht->lock );
    return rc;
}

unsigned int HT_set( HTable ht, const void *key, size_t key_size, void *data )
{
    unsigned int hash;
    AVLNode node;
    __lock( ht->lock );
    hash = ht->hf( key, key_size );
    node = AVL_insert( ht->bt[hash & UCHAR_MAX], hash, data );
    __unlock( ht->lock );
    return node ? hash : 0;
}

void *HT_get( HTable ht, const void *key, size_t key_size )
{
    return HT_get_k( ht, ht->hf( key, key_size ) );
}

void *HT_get_k( HTable ht, unsigned int key )
{
    AVLNode btn;
    __lock( ht->lock );
    btn = AVL_search( ht->bt[key & UCHAR_MAX], key );
    __unlock( ht->lock );
    return btn ? btn->data : NULL;
}

int HT_delete( HTable ht, const void *key, size_t key_size )
{
    return HT_delete_k( ht, ht->hf( key, key_size ) );
}

int HT_delete_k( HTable ht, unsigned int key )
{
    int rc;
    __lock( ht->lock );
    rc = AVL_delete( ht->bt[key & UCHAR_MAX], key );
    __unlock( ht->lock );
    return rc;
}

unsigned int HT_set_c( HTable ht, const char *key, void *data )
{
    return HT_set( ht, key, strlen( key ), data );
}

void *HT_get_c( HTable ht, const char *key )
{
    return HT_get_k( ht, ht->hf( key, strlen( key ) ) );
}

int HT_delete_c( HTable ht, const char *key )
{
    return HT_delete_k( ht, ht->hf( key, strlen( key ) ) );
}

int HT_dump( HTable ht, Tree_KeyDump kdumper, Tree_DataDump ddumper,
             FILE *handle )
{
    size_t i;
    int errors = 0;

    for( i = 0; i <= UCHAR_MAX; i++ ) {
        if( ht->bt[i]->nodes ) {
            printf( "Tree idx: %zu, ", i );
            errors += AVL_dump( ht->bt[i], kdumper, ddumper, handle );
        }
    }

    return errors;
}

#define HT_INTEGER_IMPL(tag, type) \
    unsigned int HT_set_##tag( HTable ht, type key, void *data ) { \
        return HT_set( ht, &key, sizeof(key), data ); \
    } \
    void *HT_get_##tag( HTable ht, type key) {; \
        return HT_get( ht, &key, sizeof(key) ); \
    } \
    int HT_delete_##tag( HTable ht, type key) { \
        return HT_delete( ht, &key, sizeof(key) ); \
    }

HT_INTEGER_IMPL( szt, size_t );
HT_INTEGER_IMPL( char, char );
HT_INTEGER_IMPL( uchar, unsigned char );
HT_INTEGER_IMPL( short, short );
HT_INTEGER_IMPL( ushort, unsigned short );
HT_INTEGER_IMPL( int, int );
HT_INTEGER_IMPL( uint, unsigned int );
HT_INTEGER_IMPL( long, long );
HT_INTEGER_IMPL( ulong, unsigned long );
HT_INTEGER_IMPL( llong, long long );
HT_INTEGER_IMPL( ullong, unsigned long long );

/*
 *  That's All, Folks!
 */

