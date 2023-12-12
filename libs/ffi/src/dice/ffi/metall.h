#ifndef DICE_METALLFFI_METALL_H
#define DICE_METALLFFI_METALL_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct metall_manager metall_manager;

/**
 * @brief Attempts to open the metall datastore at path
 * @param path path to datastore
 * @return pointer to metall manager on success, NULL on failure. On failure, sets errno to one of the following values:
 *      - ENOTRECOVERABLE if the given metall datastore is inconsistent or already opened as writable
 */
metall_manager *metall_open(char const *path);

/**
 * @brief Attempts to open the metall datastore at path in read only mode
 * @param path path to datastore
 * @return pointer to metall manager on success, NULL on failure. On failure, sets errno to one of the following values:
 *      - ENOTRECOVERABLE if the given metall datastore is inconsistent or already opened as writable
 */
metall_manager *metall_open_read_only(char const *path);

/**
 * @brief Attempts to create a metall datastore at path
 * @param path path at which to create a datastore
 * @return pointer to metall manager on success, NULL on failure. On failure, sets errno to one of the following values:
 *      - EEXIST if the given path already exists
 *      - ENOTRECOVERABLE if the datastore could not be created for some other reason
 */
metall_manager *metall_create(char const *path);

/**
 * @brief Creates a snapshot of the metall datastore of manager and places it at dst_path
 * @param manager manager to perform snapshot
 * @param dst_path path where to place the snapshot
 * @return true if the snapshot was successfully created otherwise false.
 */
bool metall_snapshot(metall_manager *manager, char const *dst_path);

/**
 * @brief Closes a metall manager
 */
void metall_close(metall_manager *manager);

/**
 * @brief Removes the metall datastore at path
 * @param path path to datastore to remove
 * @return true on successful removal, false otherwise. On failure, sets errno to one of the following values:
 *      - EADDRINUSE if there is a metall manager open for the given path
 *
 * @warning Behaviour is undefined if there is still a metall manager for path open
 */
bool metall_remove(char const *path);

/**
 * @brief Allocates size bytes and associates the allocated memory with a name
 * @param manager metall manager to allocate with
 * @param name A name of the allocated memory
 * @param size number of bytes to allocate
 * @return pointer to the allocated memory if sucessful otherwise returns NULL and sets errno to one of the following values
 *      - ENOMEM if the memory could not be allocated
 */
void *metall_named_malloc(metall_manager *manager, char const *name, size_t size);

/**
 * @brief Finds memory that was previously allocated using metall_named_alloc
 * @param manager manager to find the object in
 * @param name name of the allocated memory to find
 * @return pointer to the allocated memory if found. Otherwise, returns NULL and sets errno to one of the following values
 *      - ENOTENT if the object could not be found
 */
void *metall_find(metall_manager *manager, char const *name);

/**
 * @brief Frees memory previously allocated by metall_named_malloc
 * @param manager manager from which to free
 * @param name name of the allocated memory to free
 * @return true if sucessfully freed, otherwise returns false and sets errno to one of the following values
 *      - ENOENT if the referred to object does not exist
 */
bool metall_named_free(metall_manager *manager, char const *name);

/**
 * @brief Allocates size bytes
 * @param manager metall manager to allocate with
 * @param size number of bytes to allocate
 * @return pointer to the allocated memory if sucessful otherwise returns NULL and sets errno to one of the following values
 *      - ENOMEM if the memory could not be allocated
 */
void *metall_malloc(metall_manager *manager, size_t size);

/**
 * @brief Frees memory previously allocated by metall_malloc
 * @param manager manager from which to free
 * @param addr pointer to allocation
 */
void metall_free(metall_manager *manager, void *addr);

/**
 * @brief Flush data to persistent memory
 * @param manager manager to flush
 */
void metall_flush(metall_manager *manager);

#ifdef __cplusplus
}
#endif

#endif//DICE_METALLFFI_METALL_H
