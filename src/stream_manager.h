#ifndef STREAMMANAGER_H
#define STREAMMANAGER_H

#include "helpers.h"

namespace TailProduce {

    // StreamManager provides mid-level access to data in the streams.
    // It abstracts out:
    // 1) Low-level storage:
    //      Instead of raw string keys and raw byte array values,
    //      instances of StreamManager-s operates on serialized objects.
    // 2) Order keys management:
    //      StreamManager respects order keys, as well as their serialization to the storage.
    // 3) Producers and Listeners:
    //      Instead of storage-level Iterators that may hit the end and have to be re-created,
    //      StreamManager works on the scale of append-only Producers and stream-only Listeners.
    struct StreamManager {
        template <typename T_ORDER_KEY, typename T_STORAGE_KEY_BUILDER, typename T_STORAGE>
        static std::pair<T_ORDER_KEY, uint32_t> FetchHeadOrDie(const std::string& name,
                                                               const T_STORAGE_KEY_BUILDER& key_builder,
                                                               T_STORAGE& storage) {
            ::TailProduce::Storage::VALUE_TYPE storage_value;
            try {
                storage.Get(key_builder.head_storage_key, storage_value);
            } catch (const StorageException&) {
                VLOG(3) << "throw StreamDoesNotExistException();";
                throw StreamDoesNotExistException();
            }
            return T_STORAGE_KEY_BUILDER::ParseStorageKey(antibytes(storage_value));
        }
    };
};

#endif
