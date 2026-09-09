#define _POSIX_C_SOURCE 200809L

#include "flow_document.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static bool valid_name(const char *name) {
    size_t length;
    size_t i;
    if (!name || name[0] == '\0') return false;
    length = strlen(name);
    if (length >= FLOW_NAME_CAPACITY) return false;
    for (i = 0U; i < length; i++) {
        unsigned char c = (unsigned char)name[i];
        if (!(isalnum(c) || c == '_' || c == '-')) return false;
    }
    return true;
}

static bool copy_string(char *destination, size_t capacity, const char *source) {
    size_t length;
    if (!destination || !source || capacity == 0U) return false;
    length = strlen(source);
    if (length >= capacity) return false;
    if (destination == source) return true;
    memcpy(destination, source, length + 1U);
    return true;
}

static const char *node_type_name(FlowNodeType type) {
    if (type == FLOW_NODE_START) return "start";
    if (type == FLOW_NODE_SCENE) return "scene";
    if (type == FLOW_NODE_MENU) return "menu";
    return NULL;
}

static bool parse_node_type(const char *text, FlowNodeType *out_type) {
    if (!text || !out_type) return false;
    if (strcmp(text, "start") == 0) *out_type = FLOW_NODE_START;
    else if (strcmp(text, "scene") == 0) *out_type = FLOW_NODE_SCENE;
    else if (strcmp(text, "menu") == 0) *out_type = FLOW_NODE_MENU;
    else return false;
    return true;
}

static bool parse_u32(const char *text, uint32_t *out_value) {
    char *end = NULL;
    unsigned long value;
    if (!text || !out_value || text[0] == '\0' || text[0] == '-') return false;
    value = strtoul(text, &end, 10);
    if (!end || *end != '\0' || value == 0UL || value > UINT32_MAX) return false;
    *out_value = (uint32_t)value;
    return true;
}

static void trim_line(char *text) {
    size_t length;
    if (!text) return;
    length = strlen(text);
    while (length > 0U && (text[length - 1U] == '\n' ||
           text[length - 1U] == '\r')) text[--length] = '\0';
}

void flow_document_init(FlowDocument *document) {
    if (!document) return;
    memset(document, 0, sizeof(*document));
    document->nodes[0].id = 1U;
    document->nodes[0].type = FLOW_NODE_START;
    document->node_count = 1U;
    document->next_node_id = 2U;
    document->next_edge_id = 1U;
    asset_document_state_init(&document->state);
}

bool flow_document_is_dirty(const FlowDocument *document) {
    return document && asset_document_state_is_dirty(&document->state);
}

const FlowNode *flow_document_find_node(const FlowDocument *document,
                                        FlowNodeId id) {
    size_t i;
    if (!document || id == 0U) return NULL;
    for (i = 0U; i < document->node_count; i++)
        if (document->nodes[i].id == id) return &document->nodes[i];
    return NULL;
}

const FlowEdge *flow_document_find_edge(const FlowDocument *document,
                                        FlowEdgeId id) {
    size_t i;
    if (!document || id == 0U) return NULL;
    for (i = 0U; i < document->edge_count; i++)
        if (document->edges[i].id == id) return &document->edges[i];
    return NULL;
}

FlowDocumentResult flow_document_add_node(FlowDocument *document,
                                          FlowNodeType type,
                                          const char *asset_name,
                                          FlowNodeId *out_id) {
    FlowNode *node;
    if (out_id) *out_id = 0U;
    if (!document || !out_id || (type != FLOW_NODE_SCENE && type != FLOW_NODE_MENU))
        return FLOW_DOCUMENT_INVALID_ARGUMENT;
    if (!valid_name(asset_name)) return FLOW_DOCUMENT_INVALID_NAME;
    if (document->node_count >= FLOW_MAX_NODES) return FLOW_DOCUMENT_FULL;
    if (document->next_node_id == 0U || document->next_node_id == UINT32_MAX)
        return FLOW_DOCUMENT_ID_EXHAUSTED;
    node = &document->nodes[document->node_count];
    memset(node, 0, sizeof(*node));
    node->id = document->next_node_id++;
    node->type = type;
    (void)copy_string(node->asset_name, sizeof(node->asset_name), asset_name);
    document->node_count++;
    if (!asset_document_state_advance(&document->state)) {
        document->node_count--;
        document->next_node_id--;
        memset(node, 0, sizeof(*node));
        return FLOW_DOCUMENT_ID_EXHAUSTED;
    }
    *out_id = node->id;
    return FLOW_DOCUMENT_OK;
}

FlowDocumentResult flow_document_connect(FlowDocument *document,
                                         FlowNodeId source_id,
                                         const char *source_port,
                                         FlowNodeId target_id,
                                         FlowEdgeId *out_id) {
    const FlowNode *source;
    FlowEdge *edge;
    size_t i;
    if (out_id) *out_id = 0U;
    if (!document || !out_id) return FLOW_DOCUMENT_INVALID_ARGUMENT;
    source = flow_document_find_node(document, source_id);
    if (!source || !flow_document_find_node(document, target_id))
        return FLOW_DOCUMENT_MISSING_NODE;
    if (!valid_name(source_port) ||
        (source->type == FLOW_NODE_START && strcmp(source_port, "start") != 0))
        return FLOW_DOCUMENT_INVALID_PORT;
    for (i = 0U; i < document->edge_count; i++)
        if (document->edges[i].source_id == source_id &&
            strcmp(document->edges[i].source_port, source_port) == 0)
            return FLOW_DOCUMENT_DUPLICATE_SOURCE_PORT;
    if (document->edge_count >= FLOW_MAX_EDGES) return FLOW_DOCUMENT_FULL;
    if (document->next_edge_id == 0U || document->next_edge_id == UINT32_MAX)
        return FLOW_DOCUMENT_ID_EXHAUSTED;
    edge = &document->edges[document->edge_count];
    memset(edge, 0, sizeof(*edge));
    edge->id = document->next_edge_id++;
    edge->source_id = source_id;
    edge->target_id = target_id;
    (void)copy_string(edge->source_port, sizeof(edge->source_port), source_port);
    document->edge_count++;
    if (!asset_document_state_advance(&document->state)) {
        document->edge_count--;
        document->next_edge_id--;
        memset(edge, 0, sizeof(*edge));
        return FLOW_DOCUMENT_ID_EXHAUSTED;
    }
    *out_id = edge->id;
    return FLOW_DOCUMENT_OK;
}

FlowDocumentResult flow_document_set_edge_target(FlowDocument *document,
                                                 FlowEdgeId edge_id,
                                                 FlowNodeId target_id) {
    FlowEdge *edge = NULL;
    FlowNodeId previous;
    size_t i;
    FlowDocumentResult validation;
    if (!document || edge_id == 0U || target_id == 0U)
        return FLOW_DOCUMENT_INVALID_ARGUMENT;
    if (!flow_document_find_node(document, target_id))
        return FLOW_DOCUMENT_MISSING_NODE;
    for (i = 0U; i < document->edge_count; i++)
        if (document->edges[i].id == edge_id) {
            edge = &document->edges[i];
            break;
        }
    if (!edge) return FLOW_DOCUMENT_MISSING_NODE;
    if (edge->target_id == target_id) return FLOW_DOCUMENT_OK;
    previous = edge->target_id;
    edge->target_id = target_id;
    validation = flow_document_validate(document);
    if (validation != FLOW_DOCUMENT_OK ||
        !asset_document_state_advance(&document->state)) {
        edge->target_id = previous;
        return validation != FLOW_DOCUMENT_OK ? validation
                                              : FLOW_DOCUMENT_ID_EXHAUSTED;
    }
    return FLOW_DOCUMENT_OK;
}

FlowDocumentResult flow_document_validate(const FlowDocument *document) {
    bool reached[FLOW_MAX_NODES] = {false};
    size_t start_index = 0U;
    size_t start_count = 0U;
    size_t i;
    size_t j;
    bool changed;
    if (!document || document->node_count == 0U ||
        document->node_count > FLOW_MAX_NODES || document->edge_count > FLOW_MAX_EDGES)
        return FLOW_DOCUMENT_INVALID_ARGUMENT;
    for (i = 0U; i < document->node_count; i++) {
        const FlowNode *node = &document->nodes[i];
        if (node->id == 0U) return FLOW_DOCUMENT_DUPLICATE_ID;
        for (j = 0U; j < i; j++)
            if (document->nodes[j].id == node->id) return FLOW_DOCUMENT_DUPLICATE_ID;
        if (node->type == FLOW_NODE_START) {
            if (node->asset_name[0] != '\0') return FLOW_DOCUMENT_INVALID_START;
            start_index = i;
            start_count++;
        } else if ((node->type != FLOW_NODE_SCENE && node->type != FLOW_NODE_MENU) ||
                   !valid_name(node->asset_name)) return FLOW_DOCUMENT_INVALID_NAME;
        if (node->id >= document->next_node_id) return FLOW_DOCUMENT_DUPLICATE_ID;
    }
    if (start_count != 1U) return FLOW_DOCUMENT_INVALID_START;
    for (i = 0U; i < document->edge_count; i++) {
        const FlowEdge *edge = &document->edges[i];
        const FlowNode *source;
        if (edge->id == 0U) return FLOW_DOCUMENT_DUPLICATE_ID;
        if (edge->id >= document->next_edge_id) return FLOW_DOCUMENT_DUPLICATE_ID;
        for (j = 0U; j < i; j++) {
            if (document->edges[j].id == edge->id) return FLOW_DOCUMENT_DUPLICATE_ID;
            if (document->edges[j].source_id == edge->source_id &&
                strcmp(document->edges[j].source_port, edge->source_port) == 0)
                return FLOW_DOCUMENT_DUPLICATE_SOURCE_PORT;
        }
        source = flow_document_find_node(document, edge->source_id);
        if (!source || !flow_document_find_node(document, edge->target_id))
            return FLOW_DOCUMENT_MISSING_NODE;
        if (!valid_name(edge->source_port) ||
            (source->type == FLOW_NODE_START && strcmp(edge->source_port, "start") != 0))
            return FLOW_DOCUMENT_INVALID_PORT;
    }
    reached[start_index] = true;
    do {
        changed = false;
        for (i = 0U; i < document->edge_count; i++) {
            size_t source_index = FLOW_MAX_NODES;
            size_t target_index = FLOW_MAX_NODES;
            for (j = 0U; j < document->node_count; j++) {
                if (document->nodes[j].id == document->edges[i].source_id) source_index = j;
                if (document->nodes[j].id == document->edges[i].target_id) target_index = j;
            }
            if (source_index < document->node_count && target_index < document->node_count &&
                reached[source_index] && !reached[target_index]) {
                reached[target_index] = true;
                changed = true;
            }
        }
    } while (changed);
    for (i = 0U; i < document->node_count; i++)
        if (!reached[i]) return FLOW_DOCUMENT_UNREACHABLE_NODE;
    return FLOW_DOCUMENT_OK;
}

static bool write_document(FILE *file, const FlowDocument *document) {
    size_t i;
    if (fprintf(file, "flow_version=%u\nnext_node_id=%u\nnext_edge_id=%u\n",
                FLOW_DOCUMENT_VERSION, document->next_node_id,
                document->next_edge_id) < 0) return false;
    for (i = 0U; i < document->node_count; i++) {
        const FlowNode *node = &document->nodes[i];
        const char *type = node_type_name(node->type);
        if (!type || fprintf(file, "[node]\nid=%u\ntype=%s\nasset=%s\n",
                             node->id, type, node->asset_name) < 0) return false;
    }
    for (i = 0U; i < document->edge_count; i++) {
        const FlowEdge *edge = &document->edges[i];
        if (fprintf(file, "[edge]\nid=%u\nsource=%u\nport=%s\ntarget=%u\n",
                    edge->id, edge->source_id, edge->source_port,
                    edge->target_id) < 0) return false;
    }
    return true;
}

FlowDocumentResult flow_document_save_as(FlowDocument *document,
                                         const char *path) {
    char temporary[FLOW_PATH_CAPACITY + 32U];
    FILE *file;
    int fd;
    bool failed = false;
    FlowDocumentResult validation;
    if (!document || !path || path[0] == '\0' || strlen(path) >= FLOW_PATH_CAPACITY)
        return FLOW_DOCUMENT_INVALID_ARGUMENT;
    validation = flow_document_validate(document);
    if (validation != FLOW_DOCUMENT_OK) return validation;
    if (snprintf(temporary, sizeof(temporary), "%s.tmp.XXXXXX", path) >=
        (int)sizeof(temporary)) return FLOW_DOCUMENT_INVALID_ARGUMENT;
    fd = mkstemp(temporary);
    if (fd < 0) return FLOW_DOCUMENT_IO_ERROR;
    file = fdopen(fd, "w");
    if (!file) {
        (void)close(fd);
        (void)unlink(temporary);
        return FLOW_DOCUMENT_IO_ERROR;
    }
    if (!write_document(file, document)) failed = true;
    if (!failed && fflush(file) != 0) failed = true;
    if (!failed && fsync(fd) != 0) failed = true;
    if (fclose(file) != 0) failed = true;
    if (!failed && rename(temporary, path) != 0) failed = true;
    if (failed) {
        (void)unlink(temporary);
        return FLOW_DOCUMENT_IO_ERROR;
    }
    (void)copy_string(document->path, sizeof(document->path), path);
    asset_document_state_mark_saved(&document->state);
    return FLOW_DOCUMENT_OK;
}

FlowDocumentResult flow_document_save(FlowDocument *document) {
    if (!document || document->path[0] == '\0') return FLOW_DOCUMENT_INVALID_ARGUMENT;
    return flow_document_save_as(document, document->path);
}

static FlowDocumentResult parse_file(FILE *file, FlowDocument *candidate) {
    enum { SECTION_ROOT, SECTION_NODE, SECTION_EDGE } section = SECTION_ROOT;
    char line[512];
    unsigned version = 0U;
    bool have_version = false;
    bool have_next_node = false;
    bool have_next_edge = false;
    bool node_id = false, node_type = false, node_asset = false;
    bool edge_id = false, edge_source = false, edge_port = false, edge_target = false;
    memset(candidate, 0, sizeof(*candidate));
    while (fgets(line, sizeof(line), file)) {
        char *separator;
        char *key;
        char *value;
        trim_line(line);
        if (line[0] == '\0' || line[0] == '#') continue;
        if (strcmp(line, "[node]") == 0 || strcmp(line, "[edge]") == 0) {
            if (section == SECTION_NODE && !(node_id && node_type && node_asset))
                return FLOW_DOCUMENT_PARSE_ERROR;
            if (section == SECTION_EDGE && !(edge_id && edge_source && edge_port && edge_target))
                return FLOW_DOCUMENT_PARSE_ERROR;
            if (strcmp(line, "[node]") == 0) {
                if (candidate->node_count >= FLOW_MAX_NODES) return FLOW_DOCUMENT_FULL;
                section = SECTION_NODE;
                node_id = node_type = node_asset = false;
                memset(&candidate->nodes[candidate->node_count++], 0, sizeof(FlowNode));
            } else {
                if (candidate->edge_count >= FLOW_MAX_EDGES) return FLOW_DOCUMENT_FULL;
                section = SECTION_EDGE;
                edge_id = edge_source = edge_port = edge_target = false;
                memset(&candidate->edges[candidate->edge_count++], 0, sizeof(FlowEdge));
            }
            continue;
        }
        separator = strchr(line, '=');
        if (!separator) return FLOW_DOCUMENT_PARSE_ERROR;
        *separator = '\0'; key = line; value = separator + 1;
        if (section == SECTION_ROOT) {
            uint32_t parsed;
            if (strcmp(key, "flow_version") == 0 && !have_version) {
                if (!parse_u32(value, &parsed)) return FLOW_DOCUMENT_PARSE_ERROR;
                version = parsed; have_version = true;
            } else if (strcmp(key, "next_node_id") == 0 && !have_next_node) {
                if (!parse_u32(value, &candidate->next_node_id)) return FLOW_DOCUMENT_PARSE_ERROR;
                have_next_node = true;
            } else if (strcmp(key, "next_edge_id") == 0 && !have_next_edge) {
                if (!parse_u32(value, &candidate->next_edge_id)) return FLOW_DOCUMENT_PARSE_ERROR;
                have_next_edge = true;
            } else return FLOW_DOCUMENT_PARSE_ERROR;
        } else if (section == SECTION_NODE) {
            FlowNode *node = &candidate->nodes[candidate->node_count - 1U];
            if (strcmp(key, "id") == 0 && !node_id) node_id = parse_u32(value, &node->id);
            else if (strcmp(key, "type") == 0 && !node_type) node_type = parse_node_type(value, &node->type);
            else if (strcmp(key, "asset") == 0 && !node_asset) node_asset = copy_string(node->asset_name, sizeof(node->asset_name), value);
            else return FLOW_DOCUMENT_PARSE_ERROR;
            if ((strcmp(key, "id") == 0 && !node_id) || (strcmp(key, "type") == 0 && !node_type) ||
                (strcmp(key, "asset") == 0 && !node_asset)) return FLOW_DOCUMENT_PARSE_ERROR;
        } else {
            FlowEdge *edge = &candidate->edges[candidate->edge_count - 1U];
            if (strcmp(key, "id") == 0 && !edge_id) edge_id = parse_u32(value, &edge->id);
            else if (strcmp(key, "source") == 0 && !edge_source) edge_source = parse_u32(value, &edge->source_id);
            else if (strcmp(key, "port") == 0 && !edge_port) edge_port = copy_string(edge->source_port, sizeof(edge->source_port), value);
            else if (strcmp(key, "target") == 0 && !edge_target) edge_target = parse_u32(value, &edge->target_id);
            else return FLOW_DOCUMENT_PARSE_ERROR;
            if ((strcmp(key, "id") == 0 && !edge_id) || (strcmp(key, "source") == 0 && !edge_source) ||
                (strcmp(key, "port") == 0 && !edge_port) || (strcmp(key, "target") == 0 && !edge_target))
                return FLOW_DOCUMENT_PARSE_ERROR;
        }
    }
    if (ferror(file) || !have_version || !have_next_node || !have_next_edge)
        return FLOW_DOCUMENT_PARSE_ERROR;
    if (section == SECTION_NODE && !(node_id && node_type && node_asset))
        return FLOW_DOCUMENT_PARSE_ERROR;
    if (section == SECTION_EDGE && !(edge_id && edge_source && edge_port && edge_target))
        return FLOW_DOCUMENT_PARSE_ERROR;
    if (version != FLOW_DOCUMENT_VERSION) return FLOW_DOCUMENT_UNSUPPORTED_VERSION;
    asset_document_state_init(&candidate->state);
    return flow_document_validate(candidate);
}

FlowDocumentResult flow_document_load(FlowDocument *document, const char *path) {
    FlowDocument candidate;
    FlowDocumentResult result;
    FILE *file;
    if (!document || !path || path[0] == '\0' || strlen(path) >= FLOW_PATH_CAPACITY)
        return FLOW_DOCUMENT_INVALID_ARGUMENT;
    file = fopen(path, "r");
    if (!file) return FLOW_DOCUMENT_IO_ERROR;
    result = parse_file(file, &candidate);
    if (fclose(file) != 0 && result == FLOW_DOCUMENT_OK) result = FLOW_DOCUMENT_IO_ERROR;
    if (result != FLOW_DOCUMENT_OK) return result;
    if (!copy_string(candidate.path, sizeof(candidate.path), path))
        return FLOW_DOCUMENT_INVALID_ARGUMENT;
    *document = candidate;
    return FLOW_DOCUMENT_OK;
}