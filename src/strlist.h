// strlist.h: Dynamically-allocated lists of strings.

#include <stddef.h>

typedef struct _strlist
{
    /// @brief Pointer to the start of the array.
    char** begin;
    /// @brief The allocated capacity of the array. This must be >= len.
    size_t capacity;
    /// @brief The amount of strings stored in the array.
    size_t len;
}* strlist_t;

/// @brief Creates a new strlist.
/// @param list The initial list to use, or NULL.
/// @param count The initial list's count. If `list` is NULL, this is unused.
/// @return The newly-allocated `strlist_t`, or NULL if there was an error.
strlist_t
strlist_new(char** list, int len);

/// @brief Pushes a string to the end of the array.
/// @param list The list to push to.
/// @param str The string to push.
/// @return The new length of the list.
size_t
strlist_push(strlist_t list, char* str);

/// @brief Pushes all strings in a given array to this list.
/// @param list The list to push to.
/// @param strs The source list of strings.
/// @param count The number of entries to push. If this is < 0, the array will
/// be assumed to be null-terminated.
/// @return The new length of the list.
size_t
strlist_pushall(strlist_t list, char** strs, int count);

/// @brief Combines two strlists by appending `src` to `dest`.
/// @param dest The strlist to append to.
/// @param src The strlist to take entries from. This list will be freed once
/// consumed.
/// @return The new length of `dest`.
size_t
strlist_cat(strlist_t dest, strlist_t src);

/// @brief Pops a string off the end of the array.
/// @return The last string of the array, or NULL if no string was available.
char*
strlist_pop(strlist_t list);

/// @brief Claims the given index, setting it to NULL.
/// @param list The list to claim from.
/// @param idx The index of the string to claim.
/// @return The claimed string, or NULL.
char*
strlist_claim(strlist_t list, size_t idx);

/// @brief Frees a strlist created by `strlist_new`.
/// @param arr The strlist to free.
void
strlist_free(strlist_t list);

#define strlist_foreach(ent, list)                                             \
    for (char **ent##_p = (list)->begin, *ent = *ent##_p;                      \
         ent##_p < ((list)->begin + (list)->len);                              \
         ent = *(++ent##_p))
