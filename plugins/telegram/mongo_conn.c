/* -*- mode: c; c-basic-offset: 4 -*- */

/* Copyright (C) 2016-2022 Alexander Chernov <cher@ejudge.ru> */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include "mongo_conn.h"

#include "ejudge/xalloc.h"
#include "ejudge/errlog.h"
#include "ejudge/osdeps.h"

#if HAVE_LIBMONGOC - 0 > 1
#include <mongoc/mongoc.h>
#elif HAVE_LIBMONGOC - 0 > 0
#include <mongoc.h>
#elif HAVE_LIBMONGO_CLIENT - 0 == 1
#include <mongo.h>
#endif

#include <stdio.h>

#define MONGO_RETRY_TIMEOUT 60

static struct generic_conn *
free_func(struct generic_conn *gc);
static int
open_func(
        struct generic_conn *gc);
static const unsigned char *
ns_func(
        struct generic_conn *gc,
        const unsigned char *collection_name);

static struct generic_conn_iface mongo_iface =
{
    free_func,
    open_func,
    ns_func,
};

struct mongo_conn *
mongo_conn_create(void)
{
    struct mongo_conn *conn = NULL;
    XCALLOC(conn, 1);
    conn->b.vt = &mongo_iface;
    return conn;
}

static struct mongo_conn *
mongo_conn_free(struct mongo_conn *conn)
{
#if HAVE_LIBMONGOC - 0 > 0
    if (conn) {
        xfree(conn->b.database);
        xfree(conn->b.host);
        xfree(conn->b.table_prefix);
        xfree(conn->b.user);
        xfree(conn->b.password);
        if (conn->client) {
            mongoc_client_destroy(conn->client);
        }
        memset(conn, 0xff, sizeof(*conn));
        xfree(conn);
    }
    return NULL;
#elif HAVE_LIBMONGO_CLIENT - 0 == 1
    if (conn) {
        xfree(conn->database);
        xfree(conn->host);
        xfree(conn->table_prefix);
        xfree(conn->user);
        xfree(conn->password);
        if (conn->conn) {
            mongo_sync_disconnect(conn->conn);
        }
        memset(conn, 0xff, sizeof(*conn));
        xfree(conn);
    }
    return NULL;
#else
    return NULL;
#endif
}

static struct generic_conn *
free_func(struct generic_conn *gc)
{
    mongo_conn_free((struct mongo_conn *) gc);
    return NULL;
}

static int
mongo_conn_open(struct mongo_conn *state)
{
#if HAVE_LIBMONGOC - 0 > 0
    if (state->client) return 1;

    time_t current_time = time(NULL);
    if (state->last_check_time > 0 && state->last_check_time + MONGO_RETRY_TIMEOUT > current_time) {
        return 0;
    }

    if (!state->b.database) {
        if (!state->b.database) state->b.database = xstrdup("ejudge");
        state->b.show_queries = 1;
    }
    if (!state->b.table_prefix) state->b.table_prefix = xstrdup("");
    if (!state->b.host) state->b.host = xstrdup("localhost");
    if (state->b.port <= 0) state->b.port = 27017;

    unsigned char uri[1024];
    if (state->b.user && state->b.password) {
        if (snprintf(uri, sizeof(uri), "mongodb://%s:%s@%s:%d", state->b.user, state->b.password, state->b.host, state->b.port) >= sizeof(uri)) {
            err("mongodb URI is too long");
            return 0;
        }
    } else {
        if (snprintf(uri, sizeof(uri), "mongodb://%s:%d", state->b.host, state->b.port) >= sizeof(uri)) {
            err("mongodb URI is too long");
            return 0;
        }
    }

    state->last_check_time = current_time;

    mongoc_init();

    state->client = mongoc_client_new(uri);
    if (!state->client) {
        err("cannot create mongoc client");
        return 0;
    }

    mongoc_client_set_appname(state->client, "ejudge-plugin-telegram");

    return 1;
#elif HAVE_LIBMONGO_CLIENT - 0 == 1
    if (state->conn) return 1;

    time_t current_time = time(NULL);
    if (state->last_check_time > 0 && state->last_check_time + MONGO_RETRY_TIMEOUT > current_time) {
        return 0;
    }

    if (!state->database) {
        if (!state->database) state->database = xstrdup("ejudge");
        state->show_queries = 1;
    }
    if (!state->host) state->host = xstrdup("localhost");
    if (!state->table_prefix) state->table_prefix = xstrdup("");
    if (state->port <= 0) state->port = 27017;
    state->last_check_time = current_time;

    state->conn = mongo_sync_connect(state->host, state->port, 0);
    if (!state->conn) {
        err("cannot connect to mongodb: %s", os_ErrorMsg());
        return 0;
    }
    mongo_sync_conn_set_safe_mode(state->conn, 1);
    mongo_sync_conn_set_auto_reconnect(state->conn, 1);
    if (state->user && state->password) {
        if (!mongo_sync_cmd_authenticate(state->conn, state->database, state->user, state->password)) {
            err("mongodb authentification failed: %s", os_ErrorMsg());
            mongo_sync_disconnect(state->conn);
            state->conn = NULL;
            return 0;
        }
    }
    return 1;
#else
    return 0;
#endif
}

static int
open_func(
        struct generic_conn *gc)
{
    return mongo_conn_open((struct mongo_conn *) gc);
}

static const unsigned char *
mongo_conn_ns(struct mongo_conn *conn, const unsigned char *collection_name)
{
    snprintf(conn->ns, sizeof(conn->ns), "%s.%s%s", conn->b.database, conn->b.table_prefix, collection_name);
    return conn->ns;
}

static const unsigned char *
ns_func(
        struct generic_conn *gc,
        const unsigned char *collection_name)
{
    return mongo_conn_ns((struct mongo_conn *) gc, collection_name);
}
