// The test for STREAM MANAGER confirms that:
//
// 1. Stream manager binds to the data storage, be it the production one or the mock one.
// 2. Stream manager can be used to instantiate a (statically defined) set of streams.
// 3. The macros to define the set of streams do their job.
// 4. Serialization and de-serialization of entries of their respective types works.
// 5. Serialization and de-serialization of order keys of their respective types works.
// 6. Listeners and publishers can be created and behave as expected.
//
// The test for stream manager does NOT test:
//
// * The functionality of storage-level iterators.
//
//   They are handled at the lower level (data_storage.cc).
//   Stream manager operates on the level of publishers and listeners.
//
// * Making sure only one publisher can exist per stream.
//   Storage-level test that that a publisher would never overwrite data.
//   Updating the "HEAD" order key per stream.
//   Merging multiple streams maintaining strongly typed entries.
//   Ephemeral entry types as markers.
//   setTimeout()-style insertion of callbacks to be invoked by the framework later.
//
//   Framework is the level where the above is handler.

#include <gtest/gtest.h>

#include "../../src/tailproduce.h"

#include "mocks/stream_manager.h"
#include "mocks/data_storage.h"
#include "mocks/test_client.h"

using ::TailProduce::bytes;
using ::TailProduce::antibytes;
using ::TailProduce::StreamManagerParams;

// The actual test is a templated RUN_TESTS() function.
// It is used to test both the hand-crafted objects structure and the one created by a sequence of macros.
template <typename STORAGE, typename STREAM_MANAGER> void RUN_TESTS() {
    {
        // Test that STREAM_MANAGER throws an exception when attempted to be created
        // based on the storage that does not contain a definition of the `test` stream.
        STORAGE local_storage;
        std::unique_ptr<STREAM_MANAGER> p;
        ASSERT_THROW(p.reset(new STREAM_MANAGER(local_storage, StreamManagerParams())),
                     ::TailProduce::StreamDoesNotExistException);
    }

    {
        // Test that STREAM_MANAGER throws an exception when attempted to be created
        // based on the storage that contains a malformed definition of the `test` stream.
        STORAGE local_storage;
        local_storage.Set("s:test", bytes("foo"));
        std::unique_ptr<STREAM_MANAGER> p;
        ASSERT_THROW(p.reset(new STREAM_MANAGER(local_storage, StreamManagerParams())),
                     ::TailProduce::MalformedStorageHeadException);
    }

    {
        // Test that STREAM_MANAGER can be created once the storage
        // is externally set to contain the proper definition of the `test` stream.
        STORAGE local_storage;
        local_storage.Set("s:test", bytes("0000000000:0000000000"));
        STREAM_MANAGER streams_manager(local_storage, StreamManagerParams());
    }

    {
        // Test that STREAM_MANAGER initializes the storage from the parameters provided in StreamManagerParams().
        STORAGE local_storage;
        ASSERT_FALSE(local_storage.Has("s:test"));
        STREAM_MANAGER streams_manager(local_storage, StreamManagerParams().CreateStream("test", SimpleOrderKey(0)));
        ASSERT_TRUE(local_storage.Has("s:test"));
        ASSERT_EQ("0000000000:0000000000", antibytes(local_storage.Get("s:test")));
    }

    {
        // Test that entries can be serialized and de-serialized.
        SimpleEntry entry(1, "Test");
        std::ostringstream os;
        SimpleEntry::SerializeEntry(os, entry);
        std::string s = os.str();
        EXPECT_EQ("{\n    \"value0\": {\n        \"ikey\": 1,\n        \"data\": \"Test\"\n    }\n}\n", s);
        {
            SimpleEntry restored;
            std::istringstream is(s);
            SimpleEntry::DeSerializeEntry(is, restored);
            EXPECT_EQ(1, restored.ikey);
            EXPECT_EQ("Test", restored.data);
        }
    }

    {
        // Test that the order key can be serialized and de-serialized to and from fixed size byte arrays.
        SimpleEntry entry(42, "The Answer");
        SimpleOrderKey order_key =
            ::TailProduce::OrderKeyExtractorImpl<SimpleOrderKey, SimpleEntry>::ExtractOrderKey(entry);
        std::string serialized_key;
        order_key.SerializeOrderKey(serialized_key);
        EXPECT_EQ("0000000042", serialized_key);
        {
            SimpleOrderKey deserialized_order_key;
            deserialized_order_key.DeSerializeOrderKey(serialized_key);
            EXPECT_EQ(42, deserialized_order_key.ikey);
        }
    }

    STORAGE storage;
    storage.Set("s:test", bytes("0000000000:0000000000"));

    {
        // Test stream manager setup. The `test` stream should exist and be statically typed.
        STREAM_MANAGER streams_manager(storage, StreamManagerParams());

        auto entry = streams_manager.registry().Get("test");
        EXPECT_EQ("test", entry.name);
        EXPECT_EQ("SimpleEntry", entry.entry_type);
        EXPECT_EQ("SimpleOrderKey", entry.order_key_type);
        using Stream = ::TailProduce::Stream<SimpleOrderKey>;
        EXPECT_TRUE(entry.impl == static_cast<Stream*>(&streams_manager.test.stream));
        EXPECT_TRUE((std::is_same<SimpleEntry, typename STREAM_MANAGER::test_type::entry_type>::value));
        EXPECT_TRUE((std::is_same<SimpleOrderKey, typename STREAM_MANAGER::test_type::order_key_type>::value));
    }

    {
        // Test HEAD updates.
        STREAM_MANAGER streams_manager(storage, StreamManagerParams());

        // Start from zero.
        typename STREAM_MANAGER::test_type::unsafe_listener_type listener(streams_manager.test);
        typename STREAM_MANAGER::test_type::head_pair_type head;

        head = listener.GetHead();
        EXPECT_EQ(0, head.first.ikey);
        EXPECT_EQ(0, head.second);

        // Instantiating a publisher does not change HEAD.
        {
            auto& publisher = streams_manager.test_publisher;
            head = publisher.GetHead();
            EXPECT_EQ(0, head.first.ikey);
            EXPECT_EQ(0, head.second);
            head = listener.GetHead();
            EXPECT_EQ(0, head.first.ikey);
            EXPECT_EQ(0, head.second);
        }

        // Push() and PushHead() change HEAD.
        // Secondary keys are incremented automatically.
        {
            auto& publisher = streams_manager.test_publisher;

            publisher.Push(SimpleEntry(1, "foo"));
            head = publisher.GetHead();
            EXPECT_EQ(1, head.first.ikey);
            EXPECT_EQ(0, head.second);
            head = listener.GetHead();
            EXPECT_EQ(1, head.first.ikey);
            EXPECT_EQ(0, head.second);
            EXPECT_EQ(bytes("0000000001:0000000000"), storage.Get("s:test"));

            publisher.Push(SimpleEntry(1, "bar"));
            head = publisher.GetHead();
            EXPECT_EQ(1, head.first.ikey);
            EXPECT_EQ(1, head.second);
            head = listener.GetHead();
            EXPECT_EQ(1, head.first.ikey);
            EXPECT_EQ(1, head.second);
            EXPECT_EQ(bytes("0000000001:0000000001"), storage.Get("s:test"));

            publisher.PushHead(SimpleOrderKey(2));
            head = publisher.GetHead();
            EXPECT_EQ(2, head.first.ikey);
            EXPECT_EQ(0, head.second);
            head = listener.GetHead();
            EXPECT_EQ(2, head.first.ikey);
            EXPECT_EQ(0, head.second);
            EXPECT_EQ(bytes("0000000002:0000000000"), storage.Get("s:test"));

            publisher.PushHead(SimpleOrderKey(2));
            head = publisher.GetHead();
            EXPECT_EQ(2, head.first.ikey);
            EXPECT_EQ(1, head.second);
            head = listener.GetHead();
            EXPECT_EQ(2, head.first.ikey);
            EXPECT_EQ(1, head.second);
            EXPECT_EQ(bytes("0000000002:0000000001"), storage.Get("s:test"));
        }

        /*
        // TODO(dkorolev): Move this test to a dedicated, Publisher-centric test case.
        // Instantiating a publisher starting from a fixed HEAD moves HEAD there.
        {
            typename STREAM_MANAGER::test_type::publisher_type publisher(streams_manager.test, SimpleOrderKey(10));
            head = publisher.GetHead();
            EXPECT_EQ(10, head.first.ikey);
            EXPECT_EQ(0, head.second);
            head = listener.GetHead();
            EXPECT_EQ(10, head.first.ikey);
            EXPECT_EQ(0, head.second);
            EXPECT_EQ(bytes("0000000010:0000000000"), storage.Get("s:test"));
        }
        */

        // Throws an exception attempting to move HEAD backwards when doing Push().
        {
            auto& publisher = streams_manager.test_publisher;
            ASSERT_THROW(publisher.Push(SimpleEntry(0, "boom")), ::TailProduce::OrderKeysGoBackwardsException);
        }

        // Throws an exception attempting to move HEAD backwards when doing PushHead().
        {
            auto& publisher = streams_manager.test_publisher;
            ASSERT_THROW(publisher.PushHead(SimpleOrderKey(0)), ::TailProduce::OrderKeysGoBackwardsException);
        }

        /*
        // Throws an exception attempting to start a publisher starting on the order key before the most recent one.
        {
            // TODO(dkorolev): Move this test to a dedicated, Publisher-centric test case.
            typedef typename STREAM_MANAGER::test_type::publisher_type T;
            std::unique_ptr<T> p;
            ASSERT_THROW(p.reset(new T(streams_manager.test, SimpleOrderKey(0))),
                         ::TailProduce::OrderKeysGoBackwardsException);
        }
        */
    }

    {
        // Test storage schema.
        STORAGE local_storage;
        STREAM_MANAGER streams_manager(local_storage, StreamManagerParams().CreateStream("test", SimpleOrderKey(0)));

        auto& publisher = streams_manager.test_publisher;
        publisher.Push(SimpleEntry(1, "one"));
        publisher.Push(SimpleEntry(2, "two"));
        publisher.Push(SimpleEntry(3, "three"));

        EXPECT_EQ(bytes("0000000003:0000000000"), local_storage.Get("s:test"));
        EXPECT_EQ(bytes("{\n    \"value0\": {\n        \"ikey\": 1,\n        \"data\": \"one\"\n    }\n}\n"),
                  local_storage.Get("d:test:0000000001:0000000000"));
        EXPECT_EQ(bytes("{\n    \"value0\": {\n        \"ikey\": 2,\n        \"data\": \"two\"\n    }\n}\n"),
                  local_storage.Get("d:test:0000000002:0000000000"));
        EXPECT_EQ(bytes("{\n    \"value0\": {\n        \"ikey\": 3,\n        \"data\": \"three\"\n    }\n}\n"),
                  local_storage.Get("d:test:0000000003:0000000000"));
    }

    {
        // Listener test: bounded, pre-initialized with data.
        STORAGE local_storage;
        STREAM_MANAGER streams_manager(local_storage, StreamManagerParams().CreateStream("test", SimpleOrderKey(0)));

        auto& publisher = streams_manager.test_publisher;
        publisher.Push(SimpleEntry(1, "one"));
        publisher.Push(SimpleEntry(2, "two"));
        publisher.Push(SimpleEntry(3, "three"));
        publisher.Push(SimpleEntry(4, "four"));
        publisher.Push(SimpleEntry(5, "five"));

        SimpleEntry entry;
        typename STREAM_MANAGER::test_type::unsafe_listener_type listener(
            streams_manager.test, SimpleOrderKey(2), SimpleOrderKey(4));
        ASSERT_TRUE(listener.HasData());
        ASSERT_TRUE(!listener.ReachedEnd());
        listener.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(2, entry.ikey);
            EXPECT_EQ(("two"), entry.data);
        });
        listener.AdvanceToNextEntry();
        ASSERT_TRUE(listener.HasData());
        ASSERT_TRUE(!listener.ReachedEnd());
        listener.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(3, entry.ikey);
            EXPECT_EQ(("three"), entry.data);
        });
        listener.AdvanceToNextEntry();
        EXPECT_FALSE(listener.HasData());
        EXPECT_TRUE(listener.ReachedEnd());
        ASSERT_THROW(listener.AdvanceToNextEntry(), ::TailProduce::AttemptedToAdvanceListenerWithNoDataAvailable);
    }

    {
        // Listener test: bounded, pre-initialized with data, involving secondary keys.
        STORAGE local_storage;
        STREAM_MANAGER streams_manager(local_storage, StreamManagerParams().CreateStream("test", SimpleOrderKey(0)));

        auto& publisher = streams_manager.test_publisher;
        publisher.Push(SimpleEntry(42, "i0"));
        publisher.Push(SimpleEntry(42, "i1"));
        publisher.Push(SimpleEntry(42, "i2"));
        publisher.Push(SimpleEntry(42, "i3"));
        publisher.Push(SimpleEntry(42, "i4"));
        publisher.Push(SimpleEntry(42, "i5"));
        publisher.Push(SimpleEntry(42, "i6"));

        SimpleEntry entry;
        typename STREAM_MANAGER::test_type::unsafe_listener_type listener(
            streams_manager.test, std::make_pair(SimpleOrderKey(42), 2), std::make_pair(SimpleOrderKey(42), 5));
        ASSERT_TRUE(!listener.ReachedEnd());
        ASSERT_TRUE(listener.HasData());
        listener.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(42, entry.ikey);
            EXPECT_EQ(("i2"), entry.data);
        });
        listener.AdvanceToNextEntry();
        ASSERT_TRUE(listener.HasData());
        ASSERT_TRUE(!listener.ReachedEnd());
        listener.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(42, entry.ikey);
            EXPECT_EQ(("i3"), entry.data);
        });
        listener.AdvanceToNextEntry();
        ASSERT_TRUE(listener.HasData());
        ASSERT_TRUE(!listener.ReachedEnd());
        listener.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(42, entry.ikey);
            EXPECT_EQ(("i4"), entry.data);
        });
        listener.AdvanceToNextEntry();
        EXPECT_FALSE(listener.HasData());
        EXPECT_TRUE(listener.ReachedEnd());
        ASSERT_THROW(listener.AdvanceToNextEntry(), ::TailProduce::AttemptedToAdvanceListenerWithNoDataAvailable);
    }

    {
        // Listener test: appended on-the-fly, bounded.
        STORAGE local_storage;
        STREAM_MANAGER streams_manager(local_storage, StreamManagerParams().CreateStream("test", SimpleOrderKey(0)));

        SimpleEntry entry;
        auto& publisher = streams_manager.test_publisher;
        typename STREAM_MANAGER::test_type::unsafe_listener_type listener(
            streams_manager.test, SimpleOrderKey(10), SimpleOrderKey(20));

        publisher.Push(SimpleEntry(5, "five: ignored as before the beginning of the range"));
        ASSERT_TRUE(!listener.HasData());
        ASSERT_TRUE(!listener.ReachedEnd());

        publisher.Push(SimpleEntry(10, "ten"));
        ASSERT_TRUE(listener.HasData());
        ASSERT_TRUE(!listener.ReachedEnd());
        listener.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(10, entry.ikey);
            EXPECT_EQ(("ten"), entry.data);
        });
        listener.AdvanceToNextEntry();
        ASSERT_TRUE(!listener.HasData());
        ASSERT_TRUE(!listener.ReachedEnd());

        publisher.Push(SimpleEntry(15, "fifteen"));
        ASSERT_TRUE(listener.HasData());
        ASSERT_TRUE(!listener.ReachedEnd());
        listener.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(15, entry.ikey);
            EXPECT_EQ(("fifteen"), entry.data);
        });
        listener.AdvanceToNextEntry();
        ASSERT_TRUE(!listener.HasData());
        ASSERT_TRUE(!listener.ReachedEnd());

        publisher.Push(SimpleEntry(20, "twenty: ignored as part the non-included end the of range"));
        ASSERT_TRUE(!listener.HasData());
        ASSERT_TRUE(listener.ReachedEnd());
        ASSERT_THROW(listener.ProcessEntrySync([](SimpleEntry) {}), ::TailProduce::ListenerHasNoDataToRead);
        ASSERT_THROW(listener.AdvanceToNextEntry(), ::TailProduce::AttemptedToAdvanceListenerWithNoDataAvailable);
    }

    {
        // Listener test: appended on-the-fly, bounded, test that PushHead() makes ReachedEnd() return true.
        // TODO(dkorolev): Code it.
    }
}

template <typename T> class StreamManagerTest : public ::testing::Test {};

// TODO(dkorolev): Add LevelDB data storage along with the mock one.
typedef ::testing::Types<MockStreamManager<MockDataStorage>> DataStorageImplementations;
TYPED_TEST_CASE(StreamManagerTest, DataStorageImplementations);

// Runs the tests against the static framework defined by macros.
TYPED_TEST(StreamManagerTest, UserFriendlySyntaxCompiles) {
    TAILPRODUCE_STATIC_FRAMEWORK_BEGIN(StreamManagerImpl, TypeParam);
    TAILPRODUCE_STREAM(test, SimpleEntry, SimpleOrderKey);
    TAILPRODUCE_PUBLISHER(test);
    TAILPRODUCE_STATIC_FRAMEWORK_END();

    RUN_TESTS<typename StreamManagerImpl::storage_type, StreamManagerImpl>();
}

// Runs the test against explicitly defined static framework.
// Used as a reference point, as well as to ensure the macros do what they are designed for.
TYPED_TEST(StreamManagerTest, ExpandedMacroSyntaxCompiles) {
    class StreamManagerImpl {
      public:
        typedef typename TypeParam::storage_type storage_type;
        storage_type& storage;
        static storage_type& EnsureStreamsAreCreatedDuringInitialization(
            storage_type& storage,
            const ::TailProduce::StreamManagerParams& params) {
            params.Apply(storage);
            return storage;
        }
        StreamManagerImpl(storage_type& storage,
                          const ::TailProduce::StreamManagerParams& params =
                              ::TailProduce::StreamManagerParams::FromCommandLineFlags())
            : storage(EnsureStreamsAreCreatedDuringInitialization(storage, params)) {
            ::TailProduce::EnsureThereAreNoStreamsWithoutPublishers(streams_declared_, stream_publishers_declared_);
        }
        StreamManagerImpl(const StreamManagerImpl&) = delete;
        StreamManagerImpl(StreamManagerImpl&&) = delete;
        void operator=(const StreamManagerImpl&) = delete;

      private:
        using TSM = ::TailProduce::StreamManager;
        static_assert(std::is_base_of<TSM, TypeParam>::value,
                      "StreamManagerImpl: TypeParam should be derived from StreamManager.");
        using TS = ::TailProduce::Storage;
        static_assert(std::is_base_of<TS, typename TypeParam::storage_type>::value,
                      "StreamManagerImpl: TypeParam::storage_type should be derived from Storage.");
        ::TailProduce::StreamsRegistry registry_;
        std::set<std::string> streams_declared_;
        std::set<std::string> stream_publishers_declared_;

      public:
        const ::TailProduce::StreamsRegistry& registry() const {
            return registry_;
        }
        struct test_type {
            typedef SimpleEntry entry_type;
            typedef SimpleOrderKey order_key_type;
            typedef ::TailProduce::StreamInstance<entry_type, order_key_type> stream_type;
            typedef typename TypeParam::storage_type storage_type;
            typedef ::TailProduce::UnsafeListener<test_type> unsafe_listener_type;
            typedef std::pair<order_key_type, uint32_t> head_pair_type;
            typedef ::TailProduce::StorageKeyBuilder<test_type> key_builder_type;
            StreamManagerImpl* manager;
            stream_type stream;
            const std::string name;
            key_builder_type key_builder;
            head_pair_type head;
            ::TailProduce::ConfigValues cv = ::TailProduce::ConfigValues("S", "D", "Register", "LastWrite", ':');
            test_type(StreamManagerImpl* manager,
                      const char* stream_name,
                      const char* entry_type_name,
                      const char* entry_order_key_name)
                : manager(manager),
                  stream(manager->registry_, cv, stream_name, entry_type_name, entry_order_key_name),
                  name(stream_name),
                  key_builder(name),
                  head(::TailProduce::StreamManager::template FetchHeadOrDie<order_key_type,
                                                                             key_builder_type,
                                                                             storage_type>(name,
                                                                                           key_builder,
                                                                                           manager->storage)) {
                manager->streams_declared_.insert("test");
            }
        };
        test_type test = test_type(this, "test", "SimpleEntry", "SimpleOrderKey");
        struct test_publisher_type : ::TailProduce::Publisher<test_type> {
            typedef ::TailProduce::Publisher<test_type> base;
            explicit test_publisher_type(StreamManagerImpl* manager) : base(manager->test) {
                manager->stream_publishers_declared_.insert("test");
            }
        };
        test_publisher_type test_publisher = test_publisher_type(this);
    };

    RUN_TESTS<typename StreamManagerImpl::storage_type, StreamManagerImpl>();
}
