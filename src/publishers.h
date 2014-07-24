#ifndef UNSAFEPUBLISHER_H
#define UNSAFEPUBLISHER_H

#include <sstream>

// TODO(dkorolev): Rename INTERNAL_UnsafePublisher once the transition is completed.

namespace TailProduce {
    // INTERNAL_UnsafePublisher contains the logic of appending data to the streams
    // and updating their HEAD order keys.
    template <typename T> struct INTERNAL_UnsafePublisher {
        INTERNAL_UnsafePublisher() = delete;
        explicit INTERNAL_UnsafePublisher(T& stream) : stream(stream) {
        }

        INTERNAL_UnsafePublisher(T& stream, const typename T::order_key_type& order_key) : stream(stream) {
            PushHead(order_key);
        }

        void Push(const typename T::entry_type& entry) {
            typedef ::TailProduce::OrderKeyExtractorImpl<typename T::order_key_type, typename T::entry_type> impl;
            PushHead(impl::ExtractOrderKey(entry));
            std::ostringstream value_output_stream;
            T::entry_type::SerializeEntry(value_output_stream, entry);
            stream.manager->storage.Set(stream.key_builder.BuildStorageKey(stream.head),
                                        bytes(value_output_stream.str()));
        }

        void PushHead(const typename T::order_key_type& order_key) {
            typename T::head_pair_type new_head(order_key, 0);
            if (new_head.first < stream.head.first) {
                // Order keys should only be increasing.
                VLOG(3) << "throw ::TailProduce::OrderKeysGoBackwardsException();";
                throw ::TailProduce::OrderKeysGoBackwardsException();
            }
            if (!(stream.head.first < new_head.first)) {
                new_head.second = stream.head.second + 1;
            }
            // TODO(dkorolev): Perhaps more checks here?
            auto v = OrderKey::template StaticSerializeAsStorageKey<typename T::order_key_type>(new_head.first,
                                                                                                new_head.second);
            stream.manager->storage.SetAllowingOverwrite(stream.key_builder.head_storage_key, bytes(v));

            stream.head = new_head;
        }

        // TODO: PushSecondaryKey for merge usecases.

        const typename T::head_pair_type& GetHead() const {
            return stream.head;
        }

      private:
        INTERNAL_UnsafePublisher(const INTERNAL_UnsafePublisher&) = delete;
        void operator=(const INTERNAL_UnsafePublisher&) = delete;
        T& stream;
    };

    // Publisher contains the logic of appending data to the streams and updating their HEAD order keys.
    // It also handles notifying all waiting listeners that new data is now available.
    // Each stream should have one and only one Publisher, regardless of whether it is appended to externally
    // or is being populated by a running TailProduce job.
    template <typename T> struct Publisher {
        Publisher() = delete;
        explicit Publisher(T& stream) : impl(stream) {
        }

        Publisher(T& stream, const typename T::order_key_type& order_key) : impl(stream, order_key) {
        }

        void Push(const typename T::entry_type& entry) {
            impl.Push(entry);
        }

        void PushHead(const typename T::order_key_type& order_key) {
            impl.PushHead(order_key);
        }

        // TODO: PushSecondaryKey for merge usecases.

        const typename T::head_pair_type& GetHead() const {
            return impl.GetHead();
        }

      private:
        Publisher(const Publisher&) = delete;
        void operator=(const Publisher&) = delete;
        INTERNAL_UnsafePublisher<T> impl;
    };
};

#endif
