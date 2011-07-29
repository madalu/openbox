/* -*- indent-tabs-mode: nil; tab-width: 4; c-basic-offset: 4; -*-

   client_set.c for the Openbox window manager
   Copyright (c) 2011        Dana Jansens

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   See the COPYING file for a copy of the GNU General Public License.
*/

#include "client_set.h"
#include "client.h"
#include "event.h"

#include <glib.h>

struct _ObClientSet {
    GHashTable *h;
};

static void client_destroyed(ObClient *client, ObClientSet *set)
{
    g_hash_table_remove(set->h, &client->window);
}

ObClientSet* client_set_empty(void)
{
    ObClientSet *set;

    set = g_slice_new(ObClientSet);
    set->h = g_hash_table_new(g_int_hash, g_int_equal);
    client_add_destroy_notify((ObClientCallback)client_destroyed, set);
    return set;
}

ObClientSet* client_set_single(void)
{
    struct _ObClient *c;
    ObClientSet *set;

    c = event_current_target();
    if (!c) return NULL;
    set = client_set_empty();
    g_hash_table_insert(set->h, &c->window, c);
    return set;
}

/*! Returns a new set of clients with all possible client in it.*/
ObClientSet* client_set_all(void)
{
    ObClientSet *set;
    GList *it;

    if (!client_list) return NULL;
    set = client_set_empty();
    for (it = client_list; it; it = g_list_next(it)) {
        ObClient *c = it->data;
        g_hash_table_insert(set->h, &c->window, c);
    }
    return set;
}

void client_set_destroy(ObClientSet *set)
{
    client_remove_destroy_notify_data((ObClientCallback)client_destroyed, set);
    g_hash_table_destroy(set->h);
    g_slice_free(ObClientSet, set);
}

static void foreach_union(gpointer k, gpointer v, gpointer u)
{
    GHashTable *set = u;
    g_hash_table_insert(set, k, v); /* add everything in the other set */
}

/* Returns a new set which contains all clients in either @a or @b.  The sets
   @a and @b are considered freed once passed to this function.
*/
ObClientSet* client_set_union(ObClientSet *a, ObClientSet *b)
{
    g_return_val_if_fail(a != NULL, NULL);
    g_return_val_if_fail(b != NULL, NULL);

    g_hash_table_foreach(b->h, foreach_union, a->h);
    client_set_destroy(b);
    return a;
}

static gboolean foreach_intersection(gpointer k, gpointer v, gpointer u)
{
    GHashTable *set = u;
    return !g_hash_table_lookup(set, k); /* remove if not in the other set */
}

/* Returns a new set which contains all clients in both @a and @b.  The sets
   @a and @b are considered freed once passed to this function.
*/
ObClientSet* client_set_intersection(ObClientSet *a, ObClientSet *b)
{
    g_return_val_if_fail(a != NULL, NULL);
    g_return_val_if_fail(b != NULL, NULL);

    g_hash_table_foreach_remove(a->h, foreach_intersection, b->h);
    client_set_destroy(b);
    return a;
}

struct ObClientSetForeachReduce {
    ObClientSetReduceFunc f;
    gpointer data;
};

static gboolean foreach_reduce(gpointer k, gpointer v, gpointer u)
{
    ObClient *c = v;
    struct ObClientSetForeachReduce *d = u;
    return d->f(c, d->data);
}

ObClientSet* client_set_reduce(ObClientSet *set, ObClientSetReduceFunc f,
                               gpointer data)
{
    struct ObClientSetForeachReduce d;

    g_return_val_if_fail(set != NULL, NULL);
    g_return_val_if_fail(f != NULL, NULL);

    d.f = f;
    d.data = data;
    g_hash_table_foreach_remove(set->h, foreach_reduce, &d);
    return set;
}

ObClientSet* client_set_expand(ObClientSet *set, ObClientSetExpandFunc f,
                               gpointer data)
{
    GList *it;

    g_return_val_if_fail(set != NULL, NULL);
    g_return_val_if_fail(f != NULL, NULL);

    for (it = client_list; it; it = g_list_next(it)) {
        ObClient *c = it->data;
        if (!g_hash_table_lookup(set->h, &c->window) && f(c, data))
            g_hash_table_insert(set->h, &c->window, c);
    }
    return set;
}
