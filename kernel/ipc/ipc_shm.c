/*
 * kernel/ipc/ipc_shm.c
 *
 * JARVIS OS — named shared-memory segments (M7).
 *
 * Segments live in a dedicated IPC arena (not the paging frames) so the
 * IPC subsystem never reaches into the memory manager. Processes must
 * attach before reading or writing; the creator is attached on creation.
 */

#include <stdio.h>
#include <string.h>

#include "ipc_internal.h"

static jvk_ipc_segment_t* find_segment(jvk_ipc_t* ipc, const char* name)
{
    if (ipc == NULL || name == NULL) {
        return NULL;
    }
    for (int i = 0; i < JVK_IPC_MAX_SEGMENTS; i++) {
        if (ipc->segments[i].used &&
            strcmp(ipc->segments[i].name, name) == 0) {
            return &ipc->segments[i];
        }
    }
    return NULL;
}

jvk_ipc_status_t ipc_shm_create(jvk_ipc_t* ipc, const char* name, int pid,
                                int size, unsigned long ticks)
{
    if (ipc == NULL || name == NULL || name[0] == '\0' ||
        strlen(name) >= JVK_IPC_NAME_LEN) {
        return JVK_IPC_ERR_ARG;
    }
    if (find_segment(ipc, name) != NULL) {
        return JVK_IPC_ERR_EXIST;
    }
    if (size <= 0) {
        size = 128;
    }
    if (size > JVK_IPC_SEG_LEN) {
        size = JVK_IPC_SEG_LEN;
    }
    for (int i = 0; i < JVK_IPC_MAX_SEGMENTS; i++) {
        jvk_ipc_segment_t* seg = &ipc->segments[i];
        if (seg->used) {
            continue;
        }
        memset(seg, 0, sizeof(*seg));
        seg->used = 1;
        snprintf(seg->name, sizeof(seg->name), "%s", name);
        seg->owner = pid;
        seg->size = size;
        seg->created_ticks = ticks;
        if (pid > 0) {
            seg->attached[seg->attach_count++] = pid;
        }
        ipc->stats.segments_created++;
        ipc_emit(ipc, "IPC_SHM_CREATED name=%s owner=%d size=%d",
                 name, pid, size);
        return JVK_IPC_OK;
    }
    return JVK_IPC_ERR_FULL;
}

jvk_ipc_status_t ipc_shm_attach(jvk_ipc_t* ipc, const char* name, int pid)
{
    jvk_ipc_segment_t* seg = find_segment(ipc, name);
    if (seg == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (ipc_array_contains(seg->attached, seg->attach_count, pid)) {
        return JVK_IPC_OK;
    }
    if (seg->attach_count >= JVK_IPC_MAX_WAITERS) {
        return JVK_IPC_ERR_FULL;
    }
    seg->attached[seg->attach_count++] = pid;
    seg->maps++;
    ipc->stats.shm_maps++;
    ipc_emit(ipc, "IPC_SHM_ATTACH name=%s pid=%d", seg->name, pid);
    return JVK_IPC_OK;
}

jvk_ipc_status_t ipc_shm_detach(jvk_ipc_t* ipc, const char* name, int pid)
{
    jvk_ipc_segment_t* seg = find_segment(ipc, name);
    if (seg == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (!ipc_array_remove(seg->attached, &seg->attach_count, pid)) {
        return JVK_IPC_ERR_NOENT;
    }
    ipc_emit(ipc, "IPC_SHM_DETACH name=%s pid=%d", seg->name, pid);
    return JVK_IPC_OK;
}

jvk_ipc_status_t ipc_shm_write(jvk_ipc_t* ipc, const char* name, int pid,
                               const char* data, int offset, int* written)
{
    jvk_ipc_segment_t* seg = find_segment(ipc, name);
    if (written != NULL) {
        *written = 0;
    }
    if (seg == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (!ipc_array_contains(seg->attached, seg->attach_count, pid)) {
        return JVK_IPC_ERR_PERM;
    }
    if (data == NULL || offset < 0 || offset >= seg->size) {
        return JVK_IPC_ERR_ARG;
    }
    int len = (int)strlen(data);
    int room = seg->size - offset;
    if (len > room) {
        len = room;
    }
    if (len > 0) {
        memcpy(seg->data + offset, data, (size_t)len);
    }
    if (offset + len > seg->length) {
        seg->length = offset + len;
    }
    seg->writes++;
    ipc->stats.shm_writes++;
    if (written != NULL) {
        *written = len;
    }
    ipc_emit(ipc, "IPC_SHM_WRITE name=%s pid=%d bytes=%d offset=%d",
             seg->name, pid, len, offset);
    return JVK_IPC_OK;
}

jvk_ipc_status_t ipc_shm_read(jvk_ipc_t* ipc, const char* name, int pid,
                              char* out, int cap, int* out_len)
{
    jvk_ipc_segment_t* seg = find_segment(ipc, name);
    if (seg == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (!ipc_array_contains(seg->attached, seg->attach_count, pid)) {
        return JVK_IPC_ERR_PERM;
    }
    if (out == NULL || cap <= 0) {
        return JVK_IPC_ERR_ARG;
    }
    int n = seg->length < cap - 1 ? seg->length : cap - 1;
    if (n > 0) {
        memcpy(out, seg->data, (size_t)n);
    }
    out[n] = '\0';
    if (out_len != NULL) {
        *out_len = n;
    }
    seg->reads++;
    ipc->stats.shm_reads++;
    return JVK_IPC_OK;
}

jvk_ipc_status_t ipc_shm_destroy(jvk_ipc_t* ipc, const char* name, int pid)
{
    jvk_ipc_segment_t* seg = find_segment(ipc, name);
    if (seg == NULL) {
        return JVK_IPC_ERR_NOENT;
    }
    if (pid != 0 && seg->owner != pid && seg->owner != 0) {
        return JVK_IPC_ERR_PERM;
    }
    ipc_emit(ipc, "IPC_SHM_DESTROYED name=%s pid=%d", seg->name, pid);
    memset(seg, 0, sizeof(*seg));
    return JVK_IPC_OK;
}
