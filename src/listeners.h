#ifndef UNSAFELISTENERS_H
#define UNSAFELISTENERS_H

#include <memory>
#include "stream.h"

namespace TailProduce {
    // UnsafeListener contains the logic of creating and re-creating storage-level read iterators,
    // presenting data in serialized format and keeping track of HEAD order keys.
    template <typename T> struct UnsafeListener {
        UnsafeListener() = delete;

        // Unbounded.
        ~UnsafeListener() {
            VLOG(3) << this << ": UnsafeListener::~UnsafeListener();";
        }

        UnsafeListener(const T& stream, const typename T::head_pair_type& begin = typename T::head_pair_type())
            : stream(stream),
              storage(stream.manager->storage),
              cursor_key(stream.key_builder.BuildStorageKey(begin)),
              need_to_increment_cursor(false),
              has_end_key(false),
              reached_end(false) {
            VLOG(3) << this << ": UnsafeListener::UnsafeListener('" << stream.name << "', "
                    << "begin='" << cursor_key << "');";
        }
        UnsafeListener(const T& stream, const typename T::order_key_type& begin)
            : UnsafeListener(stream, std::make_pair(begin, 0)) {
        }

        // Bounded.
        UnsafeListener(const T& stream,
                       const typename T::head_pair_type& begin,
                       const typename T::head_pair_type& end)
            : stream(stream),
              storage(stream.manager->storage),
              cursor_key(stream.key_builder.BuildStorageKey(begin)),
              need_to_increment_cursor(false),
              has_end_key(true),
              end_key(stream.key_builder.BuildStorageKey(end)),
              reached_end(false) {
        }
        UnsafeListener(const T& stream,
                       const typename T::order_key_type& begin,
                       const typename T::order_key_type& end)
            : UnsafeListener(stream, std::make_pair(begin, 0), std::make_pair(end, 0)) {
        }

        UnsafeListener(UnsafeListener&&) = default;

        const typename T::head_pair_type& GetHead() const {
            return stream.head;
        }

        // Note that listeners expose HasData() / ReachedEnd(), and not Done().
        // This is because the listener, unlike the iterator, supports dynamically added data,
        // and, therefore, the standard `for (auto i = Iterator(); !i.Done(); i.Next())` loop is meaningless.

        // HasData() returns true if more data is available.
        // Can change from false to true if/when new data is available.
        bool HasData() const {
            if (reached_end) {
                VLOG(3) << this << " UnsafeListener::HasData() = false, due to reached_end = true.";
                return false;
            } else {
                if (!iterator) {
                    iterator.reset(new iterator_type(storage, cursor_key, stream.key_builder.end_stream_key));
                    if (need_to_increment_cursor && !iterator->Done()) {
                        iterator->Next();
                    }
                }
                if (iterator->Done()) {
                    iterator.reset(nullptr);
                    VLOG(3) << this << " UnsafeListener::HasData() = false, due to no data in the iterator.";
                    return false;
                }
                assert(iterator && !iterator->Done());
                if (has_end_key && iterator->Key() >= end_key) {
                    VLOG(3) << this << " UnsafeListener::HasData() = false, due to reaching the end.";
                    reached_end = true;
                    iterator.reset(nullptr);
                    return false;
                } else {
                    // TODO(dkorolev): Handle HEAD going beyond end_key resulting in ReachedEnd().
                    VLOG(3) << this << " UnsafeListener::HasData() = true.";
                    return true;
                }
            }
        }

        // ReachedEnd() returns true if the end has been reached and no data may even be read from this iterator.
        // Can only happen if the iterator has a fixed `end`, it has been reached and the HEAD of this stream
        // is beyond this end.
        bool ReachedEnd() const {
            HasData();
            return reached_end;
        }

        // ProcessEntrySync() deserealizes the entry and calls the supplied method of the respective type.
        // TODO(dkorolev): Support polymorphic types.
        template <typename T_PROCESSOR> void ProcessEntrySync(T_PROCESSOR processor, bool require_data = true) {
            if (!HasData()) {
                if (require_data) {
                    VLOG(3) << "throw ::TailProduce::ListenerHasNoDataToRead();";
                    throw ::TailProduce::ListenerHasNoDataToRead();
                } else {
                    return;
                }
            }
            if (!iterator) {
                VLOG(3) << "throw ::TailProduce::InternalError();";
                throw ::TailProduce::InternalError();
            }
            // TODO(dkorolev): Make this proof-of-concept code efficient.
            ::TailProduce::Storage::VALUE_TYPE const value = iterator->Value();
            const std::string value_as_string(value.begin(), value.end());
            std::istringstream is(value_as_string);
            typename T::entry_type entry;
            T::entry_type::DeSerializeEntry(is, entry);
            processor(entry);
        }

        // NOTE: ExportEntry() is deprecated in favor of ProcessEntrySync()
        // to allow statically supporting polymorphic types.
        // DELETED_ExportEntry() populates the passed in entry object if data is available.
        // Will throw an exception if no data is available.
        // TODO(dkorolev): Remove DELETED_ExportEntry() once the transition is completed.
        void DELETED_ExportEntry(typename T::entry_type& entry) {
            if (!HasData()) {
                VLOG(3) << "throw ::TailProduce::ListenerHasNoDataToRead();";
                throw ::TailProduce::ListenerHasNoDataToRead();
            }
            if (!iterator) {
                VLOG(3) << "throw ::TailProduce::InternalError();";
                throw ::TailProduce::InternalError();
            }
            // TODO(dkorolev): Make this proof-of-concept code efficient.
            ::TailProduce::Storage::VALUE_TYPE const value = iterator->Value();
            const std::string value_as_string(value.begin(), value.end());
            std::istringstream is(value_as_string);
            T::entry_type::DeSerializeEntry(is, entry);
        }

        // AdvanceToNextEntry() advances the listener to the next available entry.
        // Will throw an exception if no further data is (yet) available.
        void AdvanceToNextEntry() {
            if (!HasData()) {
                VLOG(3) << "throw ::TailProduce::AttemptedToAdvanceListenerWithNoDataAvailable();";
                throw ::TailProduce::AttemptedToAdvanceListenerWithNoDataAvailable();
            }
            if (!iterator) {
                VLOG(3) << "throw ::TailProduce::InternalError();";
                throw ::TailProduce::InternalError();
            }
            cursor_key = iterator->Key();
            need_to_increment_cursor = true;
            iterator->Next();
        }

      private:
        typedef typename T::storage_type storage_type;
        typedef typename T::storage_type::Iterator iterator_type;
        UnsafeListener(const UnsafeListener&) = delete;
        void operator=(const UnsafeListener&) = delete;
        const T& stream;
        storage_type& storage;
        ::TailProduce::Storage::KEY_TYPE cursor_key;
        bool need_to_increment_cursor;
        const bool has_end_key;
        ::TailProduce::Storage::KEY_TYPE const end_key;
        mutable bool reached_end;
        mutable std::unique_ptr<iterator_type> iterator;
    };
};

#endif
