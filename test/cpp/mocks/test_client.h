#ifndef TAILPRODUCE_MOCKS_TEST_CLIENT_H
#define TAILPRODUCE_MOCKS_TEST_CLIENT_H

// Example TailProduce client code.
// For this test, it generates prime numbers and aggregates those.
// The code within thi file should not depend on framework, stream manager or data storage modules.

#include "../../../src/tailproduce.h"

#include <string>
#include <cstring>
#include <sstream>
#include <iomanip>
#include "cereal/archives/binary.hpp"
#include "cereal/archives/json.hpp"

#include "cereal/types/string.hpp"
#include "cereal/types/vector.hpp"
#include "cereal/types/map.hpp"

#include "cereal/types/polymorphic.hpp"

// Ordering key.
struct SimpleOrderKey : ::TailProduce::OrderKey {
    SimpleOrderKey() = default;
    SimpleOrderKey(std::string const& streamId, TailProduce::ConfigValues const& cv) : ikey(0) {
    }
    explicit SimpleOrderKey(uint32_t key) : ikey(key) {
    }
    uint32_t ikey;

    /*
    size_t binary_OrderKeySizeInBytes() const { return 4; }
    void binary_SerializeOrderKey(uint8_t* ptr) const {
        const uint32_t value = as_msb(key);
        memcpy(ptr, &value, sizeof(value));
    }
    void binary_DeSerirializeOrderKey(const uint8_t* ptr) {
        uint32_t value;
        memcpy(&value, ptr, sizeof(value));
        key = msb_to_host_order(value);
    }
    */

    enum { size_in_bytes = 10 };  // TO GO AWAY -- D.K.
    bool operator<(const SimpleOrderKey& rhs) const {
        return ikey < rhs.ikey;
    }
    void SerializeOrderKey(::TailProduce::Storage::KEY_TYPE& ref) const {
        char tmp[11];
        snprintf(tmp, sizeof(tmp), "%010u", ikey);
        ref = tmp;
    }
    void DeSerializeOrderKey(::TailProduce::Storage::KEY_TYPE const& ref) {
        ikey = atoi(ref.c_str());
    }
    // TODO(dkorolev): A static function to extract the key w/o parsing the binary format.
};

// The SimpleEntry keeps the key/value pairs to test on.
// For this test they are { uint32, string } pairs.
struct SimpleEntry : ::TailProduce::Entry, ::TailProduce::CerealJSONSerializable<SimpleEntry> {
    SimpleEntry() = default;
    SimpleEntry(uint32_t key, ::TailProduce::Storage::KEY_TYPE const& data) : ikey(key), data(data) {
    }

    SimpleOrderKey ExtractSimpleOrderKey() const {
        return SimpleOrderKey(ikey);
    }

    // Used as order key, 1, 2, 3, etc.
    uint32_t ikey;
    // To test data extraction, "one", "two", "three", or anything else for the sake of this test.
    ::TailProduce::Storage::KEY_TYPE data;

  private:
    friend class cereal::access;
    template <class A> void serialize(A& ar) {
        ar(CEREAL_NVP(ikey), CEREAL_NVP(data));
    }
    // TOOD(dkorolev): Extracting the order key as uint32_t,
    //                 failing to extract it as any other type at combine time.
};

namespace TailProduce {
    template <> struct OrderKeyExtractorImpl<SimpleOrderKey, SimpleEntry> {
        static SimpleOrderKey ExtractOrderKey(const SimpleEntry& entry) {
            return entry.ExtractSimpleOrderKey();
        }
    };
};

/*
// An event type for a special stream type that is designed to have a special event
// sent to the Producer every N entries.
// This eliminates the need for Producer jobs to keep any counters counters.
template<typename T_ORDER_KEY> struct EveryNEntriesEvent : ::TailProduce::Entry {
    T_ORDER_KEY marker_order_key;
};

// TODO(dkorolev): Also add the functionality to make deferred calls, a.k.a. setTimeout(T_ENTRY(...), order_key).

// An example TailProduce job, effectively acting as a filter.
// TODO(dkorolev): TailProduceFilter<T_PREDICATE> should probably be a part of the standard TailProduce library.
// TODO(dkorolev): Standardize typing convention to be able to extract order key types from T_OUTPUT.
template<typename T_OUTPUT> struct PrimeNumbersTailProduce : ::TailProduce::Producer {
    void Consume(const SimpleEntry& entry, T_OUTPUT& output) {
        if (is_prime(entry.key)) {
            output.Produce(entry);
        }
    }

  private:
    static bool is_prime(uint32_t x) {
        for (uint32_t i = 2; i * i <= x; ++i) {
            if ((x % i) == 0) {
                return false;
            }
        }
        return true;
    }
};

// An example TailProduce job, effectively acting as an aggregator.
// TODO(dkorolev): TailProduceAggregator<T_MARKER> should probably be part of the standard TailProduce library.
template<typename T_ORDER_KEY, typename T_OUTPUT> struct PrimesAggregatorTailProduce : ::TailProduce::Producer {
    typedef SimpleEntry OUTPUT_TYPE;
    // T_OUTPUT is the template parameter for this instance of TailProduce job,
    // it exposes one method, T_OUTPUT::Output(const OUTPUT_TYPE& entry),
    // where OUTPUT_TYPE is defined within this class.
    void Consume(const SimpleEntry& entry, T_OUTPUT&) {
        intermediate_.push_back(entry.data);
    }
    void Consume(const EveryNEntriesEvent<T_ORDER_KEY>& marker, T_OUTPUT& output) {
        std::ostringstream os;
        for (size_t i = 0; i < intermediate_.size(); ++i) {
            if (i) {
                os << ',';
            }
            os << intermediate_[i];
        }
        intermediate_.clear();

        output.Output(SimpleEntry({ marker.marker_order_key, os.str() }));
    }
    std::vector<std::string> intermediate_;
};
*/

#endif  // TAILPRODUCE_MOCKS_TEST_CLIENT_H
