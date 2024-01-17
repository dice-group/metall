#ifndef DICE_METALLFFI_METALL_H
#define DICE_METALLFFI_METALL_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct copperr_manager copperr_manager;

/**
 * @brief Attempts to open the copperr datastore at path
 * @param path path to datastore
 * @return pointer to copperr manager on success, NULL on failure. On failure, sets errno to one of the following values:
 *      - ENOTRECOVERABLE if the given copperr datastore is inconsistent or already opened as writable
 */
copperr_manager *copperr_open(char const *path);

/**
 * @brief Attempts to open the copperr datastore at path in read only mode
 * @param path path to datastore
 * @return pointer to copperr manager on success, NULL on failure. On failure, sets errno to one of the following values:
 *      - ENOTRECOVERABLE if the given copperr datastore is inconsistent or already opened as writable
 */
copperr_manager *copperr_open_read_only(char const *path);

/**
 * @brief Attempts to create a copperr datastore at path
 * @param path path at which to create a datastore
 * @return pointer to copperr manager on success, NULL on failure. On failure, sets errno to one of the following values:
 *      - EEXIST if the given path already exists
 *      - ENOTRECOVERABLE if the datastore could not be created for some other reason
 */
copperr_manager *copperr_create(char const *path);

/**
 * @brief Creates a snapshot of the copperr datastore of manager and places it at dst_path
 * @param manager manager to perform snapshot
 * @param dst_path path where to place the snapshot
 * @return true if the snapshot was successfully created otherwise false.
 */
bool copperr_snapshot(copperr_manager *manager, char const *dst_path);

/**
 * @brief Closes a copperr manager
 */
void copperr_close(copperr_manager *manager);

/**
 * @brief Removes the copperr datastore at path
 * @param path path to datastore to remove
 * @return true on successful removal, false otherwise. On failure, sets errno to one of the following values:
 *      - EADDRINUSE if there is a copperr manager open for the given path
 *
 * @warning Behaviour is undefined if there is still a copperr manager for path open
 */
bool copperr_remove(char const *path);

/**
 * @brief Allocates size bytes and associates the allocated memory with a name
 * @param manager copperr manager to allocate with
 * @param name A name of the allocated memory
 * @param size number of bytes to allocate
 * @return pointer to the allocated memory if sucessful otherwise returns NULL and sets errno to one of the following values
 *      - ENOMEM if the memory could not be allocated
 */
void *copperr_named_malloc(copperr_manager *manager, char const *name, size_t size);

/**
 * @brief Finds memory that was previously allocated using copperr_named_alloc
 * @param manager manager to find the object in
 * @param name name of the allocated memory to find
 * @return pointer to the allocated memory if found. Otherwise, returns NULL and sets errno to one of the following values
 *      - ENOTENT if the object could not be found
 */
void *copperr_find(copperr_manager *manager, char const *name);

/**
 * @brief Frees memory previously allocated by copperr_named_malloc
 * @param manager manager from which to free
 * @param name name of the allocated memory to free
 * @return true if sucessfully freed, otherwise returns false and sets errno to one of the following values
 *      - ENOENT if the referred to object does not exist
 */
bool copperr_named_free(copperr_manager *manager, char const *name);

/**
 * @brief Allocates size bytes
 * @param manager copperr manager to allocate with
 * @param size number of bytes to allocate
 * @return pointer to the allocated memory if sucessful otherwise returns NULL and sets errno to one of the following values
 *      - ENOMEM if the memory could not be allocated
 */
void *copperr_malloc(copperr_manager *manager, size_t size);

/**
 * @brief Frees memory previously allocated by copperr_malloc
 * @param manager manager from which to free
 * @param addr pointer to allocation
 */
void copperr_free(copperr_manager *manager, void *addr);

/**
 * @brief Flush data to persistent memory
 * @param manager manager to flush
 */
void copperr_flush(copperr_manager *manager);

#ifdef __cplusplus
}
#endif

#endif//DICE_METALLFFI_METALL_H
