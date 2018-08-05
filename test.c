#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "src/json.h"

struct Board {
    const char *name;
    const char *id;
    const char *shortUrl;
};

int keyCmp(const char *key, jsmntok_t field, const char *json) {
    if (field.type != JSMN_STRING)
        return 0;

    int fieldLen = field.end - field.start;
    for (size_t i = 0; i < fieldLen; i += 1) {
        if (key[i] != json[field.start+i])
            return 0;
    }

    return 1;
}

int extractString(
    const char *key,
    jsmntok_t field,
    jsmntok_t *child,
    const char *json,
    const char **out
) {
    if (field.type != JSMN_STRING)
        return 0;

    int fieldLen = field.end - field.start;
    for (size_t i = 0; i < fieldLen; i += 1) {
        if (key[i] != json[field.start+i])
            return 0;
    }

    if (child->type != JSMN_STRING) {
        return -3;
    }

    *out = strndup(json + child->start, child->end - child->start);
    return 1;
}

void skipTokens(jsmntok_t *tokens, int *index) {
    switch (tokens[*index].type) {
    case JSMN_PRIMITIVE:
    case JSMN_STRING:
    case JSMN_UNDEFINED:
        *index = (*index)+1;
        break;

    case JSMN_ARRAY: {
        int start = *index;
        jsmntok_t arry = tokens[start++];

        for (size_t i = 0; i < arry.size; i++) {
            skipTokens(tokens, &start);
        }

        *index = start;
    } break;

    case JSMN_OBJECT: {
        int start = *index;
        jsmntok_t obj = tokens[start++];

        for (size_t i = 0; i < obj.size; i++) {
            skipTokens(tokens, &start); // key
            skipTokens(tokens, &start); // value
        }

        *index = start;
    } break;
    }
}

int parseBoards(const char *json, struct Board **boardsOut) {
    jsmn_parser parser;
    jsmn_init(&parser);

    size_t jsonLen = strlen(json);
    int tokenCount = jsmn_parse(&parser, json, jsonLen, NULL, 0);
    if (tokenCount < 1) {
        printf("Failed to parse json: %d\n", tokenCount);
        return -6;
    }

    printf("Response had %d tokens\n", tokenCount);

    jsmntok_t *tokens = calloc(tokenCount, sizeof(jsmntok_t));
    if (!tokens) {
        return -4;
    }

    jsmn_init(&parser);
    int count;
    count = jsmn_parse(&parser, json, strlen(json), tokens, tokenCount);
    if (count < 0) {
        printf("Failed to load tokens: %d\n", count);
        return -5;
    }

    if (tokens[0].type != JSMN_ARRAY) {
        printf("json type: %d\n", tokens[0].type);
        return -1;
    }

    int boardCount = tokens[0].size;

    // TODO(Brett): don't malloc, use allocator instead
    struct Board *boards = calloc(boardCount, sizeof(struct Board));
    
    int offset = 1;
    for (size_t boardIndex = 0; boardIndex < boardCount; boardIndex++) {
        struct Board *board = boards+boardIndex;
        jsmntok_t obj = tokens[offset++];

        if (obj.type != JSMN_OBJECT) {
            // TODO(Brett): error label and cleanup allocations
            return -2;
        }

        int fields = obj.size;

        for (size_t i = 0; i < fields; i += 1) {
            jsmntok_t field = tokens[offset++];

            extractString("name", field, &tokens[offset], json, &board->name);
            extractString("id", field, &tokens[offset], json, &board->id);
            extractString("shortUrl", field, &tokens[offset], json, &board->shortUrl);

            skipTokens(tokens, &offset);
        }
    }

    *boardsOut = boards;
    return boardCount;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: %s <json>\n", argv[0]);
    }

    FILE *file = fopen(argv[1], "r");
    if (!file) {
        printf("Failed to open file\n");
        return 1;
    }

    char *json = malloc(1024 * 1024);
    int bytesRead = fread(json, 1, 1024*1024, file);
    printf("Read %d bytes: \n", bytesRead);

    struct Board *boards;
    int boardCount = parseBoards(json, &boards);
    if (boardCount < 0) {
        printf("Failed to parse response (%d)\n", boardCount);
        return 1;
    }

    printf("Parsed %d boards:\n", boardCount);
    for (size_t i = 0; i < boardCount; i += 1) {
        struct Board board = boards[i];
        printf("  id: '%s', name: '%s', shortUrl: '%s'\n", board.id, board.name, board.shortUrl);
    }

    return 0;
}
