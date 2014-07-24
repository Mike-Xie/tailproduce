#ifndef UNSAFEPUBLISHER_H
#define UNSAFEPUBLISHER_H

#include <sstream>

// TODO(dkorolev): Rename INTERNAL_UnsafePublisher once the transition is completed.

namespace TailProduce {
    inline void EnsureThereAreNoStreamsWithoutPublishers(const std::set<std::string>& streams_declared,
                                                         const std::set<std::string>& stream_publishers_declared) {
        std::vector<std::string> diff;
        std::set_difference(streams_declared.begin(),
                            streams_declared.end(),
                            stream_publishers_declared.begin(),
                            stream_publishers_declared.end(),
                            std::back_inserter(diff));
        if (!diff.empty()) {
            std::ostringstream os;
            for (const auto cit : diff) {
                os << ',' << cit;
                VLOG(3) << "Stream '" << cit << "' has been declared but has no writer associated with it.";
            }
            throw ::TailProduce::StreamHasNoWriterDefinedException(os.str().substr(1));
        }
    }

    // INTERNAL_UnsafePublisher contains the logic of appending data to the streams
    // and updating their HEAD order keys.
    template <typename T> struct INTERNAL_UnsafePublisher {
        explicit INTERNAL_UnsafePublisher(T& stream) : stream(stream) {
        }

        INTERNAL_UnsafePublisher(INTERNAL_UnsafePublisher&&) = default;

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

        T& stream;

        INTERNAL_UnsafePublisher() = delete;
        INTERNAL_UnsafePublisher(const INTERNAL_UnsafePublisher&) = delete;
        void operator=(const INTERNAL_UnsafePublisher&) = delete;
    };

    // Publisher contains the logic of appending data to the streams and updating their HEAD order keys.
    // It also handles notifying all waiting listeners that new data is now available.
    // Each stream should have one and only one Publisher, regardless of whether it is appended to externally
    // or is being populated by a running TailProduce job.
    template <typename T> struct Publisher {
        explicit Publisher(T& stream) : impl(stream) {
        }
        Publisher(Publisher&&) = default;

        Publisher(T& stream, const typename T::order_key_type& order_key) : impl(stream, order_key) {
        }

        void Push(const typename T::entry_type& entry) {
            impl.Push(entry);
            impl.stream.subscriptions.PokeAll();
        }

        void PushHead(const typename T::order_key_type& order_key) {
            impl.PushHead(order_key);
            impl.stream.subscriptions.PokeAll();
        }

        // TODO: PushSecondaryKey for merge usecases.

        const typename T::head_pair_type& GetHead() const {
            return impl.GetHead();
        }

        Publisher() = delete;
        Publisher(const Publisher&) = delete;
        void operator=(const Publisher&) = delete;

        INTERNAL_UnsafePublisher<T> impl;
    };
};

#endif
