/* ejemplo_hiredis.c - cliente de Valkey con hiredis (D477).
 *
 *   tcc ejemplo_hiredis.c -lhiredis -o ejemplo_hiredis      (con valkey-server en marcha)
 *   ejemplo_hiredis [dirección [puerto]]
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hiredis/hiredis.h>

static void mostrar(const char *que, redisReply *r)
{
    if (!r) { printf("%s: sin respuesta\n", que); return; }
    switch (r->type) {
    case REDIS_REPLY_STRING: case REDIS_REPLY_STATUS: printf("%s: %s\n", que, r->str); break;
    case REDIS_REPLY_INTEGER: printf("%s: %lld\n", que, r->integer); break;
    case REDIS_REPLY_NIL: printf("%s: (nil)\n", que); break;
    case REDIS_REPLY_ERROR: printf("%s: ERROR %s\n", que, r->str); break;
    case REDIS_REPLY_ARRAY:
        printf("%s:", que);
        for (size_t i = 0; i < r->elements; i++)
            printf(" %s", r->element[i]->str ? r->element[i]->str : "?");
        printf("\n");
        break;
    default: printf("%s: tipo %d\n", que, r->type);
    }
    freeReplyObject(r);
}

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "127.0.0.1";
    int puerto = argc > 2 ? atoi(argv[2]) : 6379;
    redisContext *c = redisConnect(dir, puerto);
    if (!c || c->err) {
        printf("no conecta: %s\n", c ? c->errstr : "sin memoria");
        return 1;
    }
    mostrar("PING", redisCommand(c, "PING"));
    mostrar("SET", redisCommand(c, "SET %s %s", "c:clave", "valor desde C"));
    mostrar("GET", redisCommand(c, "GET %s", "c:clave"));
    redisCommand(c, "DEL c:contador");
    redisCommand(c, "INCR c:contador");
    mostrar("INCRBY", redisCommand(c, "INCRBY c:contador %d", 41));
    redisCommand(c, "DEL c:lista");
    redisCommand(c, "RPUSH c:lista a b c");
    mostrar("LRANGE", redisCommand(c, "LRANGE c:lista 0 -1"));
    redisCommand(c, "DEL c:hash");
    redisCommand(c, "HSET c:hash nombre SaguiOS ano 2026");
    mostrar("HGETALL", redisCommand(c, "HGETALL c:hash"));
    /* un valor binario (con ceros) con %b */
    redisReply *r = redisCommand(c, "SET c:bin %b", "a\0b", (size_t)3);
    freeReplyObject(r);
    r = redisCommand(c, "STRLEN c:bin");
    mostrar("STRLEN del valor binario", r);
    redisFree(c);
    return 0;
}
