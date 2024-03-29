/*
 * ttree.c, part of "trees" project
 *
 *  Created on: 10.05.2015
 *  Author: Vsevolod Lutovinov <klopp@yandex.ru>
 */
#include "ttree.h"
#include <string.h>
#include <ctype.h>

/*
 *  Internal, create empty node:
 */
static TTNode _TT_create_node( char c )
{
    TTNode node = Calloc( sizeof( struct _TTNode ), 1 );

    if( !node ) {
        return NULL;
    }

    node->splitter = c;
    return node;
}

/*
 *  Create empty tree:
 */
TTree TT_create( Tree_Flags flags, Tree_Destroy destructor )
{
    TTree tree = Calloc( sizeof( struct _TernaryTree ), 1 );

    if( tree ) {
        tree->head = _TT_create_node( 0 );

        if( tree->head ) {
            tree->flags = flags;

            if( destructor ) {
                tree->destructor = destructor;
            }
            else if( flags & T_FREE_DEFAULT ) {
                tree->destructor = T_Free;
            }

            __initlock( tree->lock );
            return tree;
        }

        Free( tree );
    }

    return NULL;
}

/*
 *  Destroy tree / delete tree node stuff:
 */
static void _TT_destroy( TTNode node, TTree tree, int delnode )
{
    if( node->left ) {
        _TT_destroy( node->left, tree, delnode );
    }

    if( node->mid ) {
        _TT_destroy( node->mid, tree, delnode );
    }

    if( node->right ) {
        _TT_destroy( node->right, tree, delnode );
    }

    if( node->data && tree->destructor ) {
        tree->destructor( node->data );
    }

    Free( node->key );
    tree->keys--;
    tree->nodes--;
    memset( node, 0, sizeof( struct _TTNode ) );

    if( delnode ) {
        Free( node );
    }
}
void TT_destroy( TTree tree )
{
    if( tree->head ) {
        __lock( tree->lock );
        _TT_destroy( tree->head, tree, 1 );
        __unlock( tree->lock );
    }

    memset( tree, 0, sizeof( struct _TernaryTree ) );
    Free( tree );
}
void TT_clear( const TTree tree )
{
    __lock( tree->lock );
    _TT_destroy( tree->head->mid, tree, 1 );
    tree->head->mid = NULL;
    __unlock( tree->lock );
}

/*
 *  Search nodes stuff:
 */
static TTNode _TT_search( TTNode node, const char *s, Tree_Flags flags )
{
    TTNode ptr = node;

    if( !ptr || !s || !*s ) {
        return NULL;
    }

    while( *s && ( ptr && ( ptr->splitter || ptr->key ) ) ) {
        char c = ( flags & T_NOCASE ) ? tolower( *s ) : *s;

        if( c < ptr->splitter ) {
            ptr = ptr->left;
        }
        else if( c > ptr->splitter ) {
            ptr = ptr->right;
        }
        else {
            s++;

            if( *s ) {
                ptr = ptr->mid;
            }
        }
    }

    return ( ptr && ptr->key ) ? ptr : NULL;
}


int TT_del_node( const TTree tree, const char *key )
{
    TTNode node;

    if( !tree || !tree->head || !key || !*key ) {
        return 0;
    }

    node = _TT_search( tree->head->mid, key, tree->flags );

    if( node ) {
        __lock( tree->lock );
        _TT_destroy( node, tree, 0 );
        __unlock( tree->lock );
        return 1;
    }

    return 0;
}
int TT_del_key( const TTree tree, const char *key )
{
    TTNode node;

    if( !tree || !tree->head || !key || !*key ) {
        return 0;
    }

    node = _TT_search( tree->head->mid, key, tree->flags );

    if( node ) {
        __lock( tree->lock );

        if( node->data && tree->destructor ) {
            tree->destructor( node->data );
        }

        node->data = NULL;
        Free( node->key );
        tree->keys--;
        node->key = NULL;
        __unlock( tree->lock );
        return 1;
    }

    return 0;
}

TTNodeConst TT_search( const TTree tree, const char *s )
{
    TTNode node;

    if( !tree || !tree->head || !s || !*s ) {
        return NULL;
    }

    __lock( tree->lock );
    node = _TT_search( tree->head->mid, s, tree->flags );
    __unlock( tree->lock );
    return node;
}

/*
 *  Insert nodes stuff:
 */
static TTNode _TT_insert( TTNode node, const char *s, size_t pos, void *data,
                          TTree tree, size_t depth )
{
    char c;

    if( !tree || !tree->head || !s || !*s ) {
        return NULL;
    }

    c = ( tree->flags & T_NOCASE ) ? tolower( s[pos] ) : s[pos];

    if( !node ) {
        node = _TT_create_node( c );

        if( !node ) {
            return NULL;
        }

        node->depth = depth;
        tree->nodes++;
        /*if( tree->depth < depth ) tree->depth = depth;*/
    }
    else if( !node->splitter && !node->key ) {
        memset( node, 0, sizeof( struct _TTNode ) );
        node->splitter = c;
        node->depth = depth;
        /*if( tree->depth < depth ) tree->depth = depth;*/
    }

    if( c < node->splitter ) node->left = _TT_insert( node->left, s, pos, data,
                                              tree, depth + 1 );

    if( c == node->splitter ) {
        if( *( s + pos + 1 ) > 0 ) {
            node->mid = _TT_insert( node->mid, s, pos + 1, data, tree,
                                    depth + 1 );
        }
        else {
            if( !node->key ) {
                node->key = Strdup( s );

                if( !node->key ) {
                    return NULL;
                }

                tree->keys++;
            }

            if( tree->flags & T_INSERT_REPLACE ) {
                if( node->data && tree->destructor ) tree->destructor(
                        node->data );

                node->data = data;
            }

            /*
             * do not free data
             */
        }
    }

    if( c > node->splitter ) node->right = _TT_insert( node->right, s, pos,
                                               data, tree, depth + 1 );

    return node;
}
TTNodeConst TT_insert( const TTree tree, const char *s, void *data )
{
    TTNode node;

    if( !tree || !tree->head || !s || !*s ) {
        return NULL;
    }

    __lock( tree->lock );
    tree->head->mid = _TT_insert( tree->head->mid, s, 0, data, tree, 1 );

    if( !tree->head->mid ) {
        __unlock( tree->lock );
        return NULL;
    }

    node = ( tree->flags & T_INSERT_FAST ) ?
           tree->head : _TT_search( tree->head->mid, s, tree->flags );
    __unlock( tree->lock );
    return node;
}

/*
 *  Tree walking stuff:
 */
static void _TT_walk( TTNodeConst node, TT_Walk walker, void *data )
{
    if( node ) {
        _TT_walk( node->left, walker, data );
        _TT_walk( node->mid, walker, data );
        _TT_walk( node->right, walker, data );
        walker( node, data );
    }
}
void _TT_walk_asc( TTNodeConst node, TT_Walk walker, void *data )
{
    if( node ) {
        //        walker( node, data );
        _TT_walk_asc( node->left, walker, data );
        walker( node, data );
        _TT_walk_asc( node->mid, walker, data );
        _TT_walk_asc( node->right, walker, data );
    }
}
static void _TT_walk_desc( TTNodeConst node, TT_Walk walker, void *data )
{
    if( node ) {
        _TT_walk_desc( node->right, walker, data );
        _TT_walk_desc( node->mid, walker, data );
        walker( node, data );
        _TT_walk_desc( node->left, walker, data );
        //        walker( node, data );
    }
}
void TT_walk( const TTree tree, TT_Walk walker, void *data )
{
    if( tree ) {
        __lock( tree->lock );
        _TT_walk( tree->head, walker, data );
        __unlock( tree->lock );
    }
}
void TT_walk_asc( const TTree tree, TT_Walk walker, void *data )
{
    if( tree ) {
        __lock( tree->lock );
        _TT_walk_asc( tree->head, walker, data );
        __unlock( tree->lock );
    }
}
void TT_walk_desc( const TTree tree, TT_Walk walker, void *data )
{
    if( tree ) {
        __lock( tree->lock );
        _TT_walk_desc( tree->head, walker, data );
        __unlock( tree->lock );
    }
}

/*
 *  Tree information stuff, get depth:
 */
static void _TT_depth( TTNodeConst node, void *data )
{
    if( node->depth > *( size_t * )data ) {
        ( *( size_t * )data ) = node->depth;
    }
}
size_t TT_depth( const TTree tree )
{
    size_t max = 0;

    if( tree ) {
        TT_walk( tree, _TT_depth, &max );
    }

    return max;
}

/*
 *  Get sorted data from tree:
 */
static void _TT_data( TTNodeConst node, void *data )
{
    if( node->key ) {
        struct {
            size_t max;
            size_t idx;
            TT_Data data;
        }*ptr = data;

        if( ptr->idx < ptr->max ) {
            ptr->data[ptr->idx].key = node->key;
            ptr->data[ptr->idx].data = node->data;
            ptr->idx++;
        }
    }
}

TT_DataConst TT_data( const TTree tree, size_t *count )
{
    struct {
        size_t max;
        size_t idx;
        TT_Data data;
    } data =
    { 0 };

    if( count ) {
        *count = 0;
    }

    if( !tree ) {
        return NULL;
    }

    data.data = Calloc( sizeof( struct _TT_Data ), tree->keys + 1 );

    if( !data.data ) {
        return NULL;
    }

    data.max = ( ( size_t ) - 1 );
    TT_walk_asc( tree, _TT_data, &data );

    if( count ) {
        *count = data.idx;
    }

    return data.data;
}

void _TS_data( TTNodeConst node, void *data )
{
    if( node->key ) {
        struct {
            size_t max;
            size_t idx;
            char **data;
        }*ptr = data;

        if( ptr->idx < ptr->max ) {
            ptr->data[ptr->idx] = node->key;
            ptr->idx++;
        }
    }
}
char **TS_data( const TTree tree, size_t *count )
{
    struct {
        size_t max;
        size_t idx;
        char **data;
    } data =
    { 0 };

    if( count ) {
        *count = 0;
    }

    if( !tree ) {
        return NULL;
    }

    data.data = Calloc( sizeof( char * ), tree->keys + 1 );

    if( !data.data ) {
        return NULL;
    }

    data.max = ( ( size_t ) - 1 );
    TT_walk_asc( tree, _TS_data, &data );

    if( count ) {
        *count = data.idx;
    }

    return data.data;
}

/*
 *  Dump tree stuff:
 */
static void _TT_dump( const TTNode node, Tree_DataDump dumper, char *indent,
                      int last,
                      FILE *handle )
{
    size_t strip = 0;

    if( node->splitter ) {
        strip = T_Indent( indent, last, handle );

        if( node->key ) {
            fprintf( handle, "%c => [%s]",
                     ( isprint( node->splitter ) ? node->splitter : '?' ),
                     node->key );
        }
        else {
            fprintf( handle, "%c => ()",
                     ( isprint( node->splitter ) ? node->splitter : '?' ) );
        }

        if( dumper ) {
            dumper( node->data, handle );
        }

        fprintf( handle, "\n" );
    }

    if( node->left ) _TT_dump( node->left, dumper, indent,
                                   ( node->right || node->mid ) ? 0 : 1, handle );

    if( node->mid ) _TT_dump( node->mid, dumper, indent, node->right ? 0 : 1,
                                  handle );

    if( node->right ) {
        _TT_dump( node->right, dumper, indent, 1, handle );
    }

    if( strip ) {
        indent[strip] = 0;
    }
}

int TT_dump( TTree tree, Tree_DataDump dumper, FILE *handle )
{
    size_t depth = TT_depth( tree );
    char *buf = Calloc( depth + 1, 2 );

    if( buf ) {
        fprintf( handle, "nodes: %zu, keys: %zu, depth: %zu\n",
                 tree->nodes/*TT_nodes( tree )*/, tree->keys/*TT_keys( tree )*/,
                 TT_depth( tree ) );
        __lock( tree->lock );
        _TT_dump( tree->head, dumper, buf, 0, handle );
        __unlock( tree->lock );
        Free( buf );
        return 1;
    }

    return 0;
}

/*
 *  Lookup stuff:
 */
TTNode __TT_lookup( TTNode node, const char *s, Tree_Flags flags )
{
    TTNode ptr = node;

    while( *s && ( ptr && ptr->splitter ) ) {
        char c = ( flags & T_NOCASE ) ? tolower( *s ) : *s;

        if( c < ptr->splitter ) {
            ptr = ptr->left;
        }
        else if( c > ptr->splitter ) {
            ptr = ptr->right;
        }
        else {
            s++;

            if( *s ) {
                ptr = ptr->mid;
            }
        }
    }

    return ptr;
}

TT_Data _TT_lookup( TTree tree, const char *prefix, size_t max,
                    size_t *count )
{
    TTNode node;
    struct {
        size_t max;
        size_t idx;
        TT_Data data;
    } data =
    { 0 };

    if( count ) {
        *count = 0;
    }

    if( !tree || !prefix || !*prefix ) {
        return NULL;
    }

    node = __TT_lookup( tree->head->mid, prefix, tree->flags );

    if( !node || !node->mid ) {
        return NULL;
    }

    if( !max ) {
        max = tree->keys/*TT_keys( tree )*/;
    }

    data.data = Calloc( sizeof( struct _TT_Data ), max + 1 );

    if( !data.data ) {
        return NULL;
    }

    data.max = max;
    _TT_walk_asc( node->mid, _TT_data, &data );

    if( count ) {
        *count = data.idx;
    }

    return data.data;
}

TTree TT_lookup_tree( TTree tree, const char *prefix )
{
    TTree rc = TT_create( tree->flags, NULL );
    TT_Data data, ptr;

    if( !rc ) {
        return NULL;
    }

    __lock( tree->lock );
    data = _TT_lookup( tree, prefix, 0, NULL );
    ptr = data;

    while( ptr && ptr->key ) {
        rc->head->mid = _TT_insert( rc->head->mid, ptr->key, 0, ptr->data, rc,
                                    1 );
        ptr++;
    }

    __unlock( tree->lock );
    Free( data );
    return rc;
}

