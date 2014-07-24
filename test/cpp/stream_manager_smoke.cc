// The smoke test to illustrate the functionality of the stream manager.
// Please refer to stream_manager.cc for a more complete test.

#include <gtest/gtest.h>

#include "../../src/tailproduce.h"

#include "mocks/stream_manager.h"
#include "mocks/data_storage.h"
#include "mocks/test_client.h"

using ::TailProduce::bytes;
using ::TailProduce::StreamManagerParams;

// Keep in mind that Stats and Aggregator must be two different classes,
// since an instance of Aggregator will be cloned while being passed to the listener.
struct Stats {
    size_t count = 0;
    std::string data = "";
};

struct Aggregator {
    Stats& stats;
    explicit Aggregator(Stats& stats) : stats(stats) {
    }
    void operator()(const SimpleEntry& entry) {
        std::ostringstream os;
        os << '[' << stats.count << "]:{" << entry.ikey << ",'" << entry.data << "'}";
        if (stats.count) {
            stats.data.append(",");
        }
        stats.data.append(os.str());
        ++stats.count;
    }
};

TEST(StreamManagerSmokeTest, SmokeTest) {
    TAILPRODUCE_STATIC_FRAMEWORK_BEGIN(Impl, MockStreamManager<MockDataStorage>);
    TAILPRODUCE_STREAM(test, SimpleEntry, SimpleOrderKey);
    TAILPRODUCE_PUBLISHER(test);
    TAILPRODUCE_STATIC_FRAMEWORK_END();

    MockDataStorage storage;

    {
        // Mimic the 1st run with the command line flag set to initialize the stream in the storage.
        Impl streams_manager(storage, StreamManagerParams().CreateStream("test", SimpleOrderKey(0)));
    }

    {
        // Mimic the consecutive run(s) that rely on the fact that the stream exists.
        Impl streams_manager(storage, StreamManagerParams());

        Stats stats;
        auto test_listener_existence_scope = streams_manager.new_scoped_test_listener(Aggregator(stats));

        SimpleEntry entry;

        typename Impl::test_type::publisher_type publisher(streams_manager.test);
        typename Impl::test_type::INTERNAL_unsafe_listener_type listener_all(streams_manager.test);
        typename Impl::test_type::INTERNAL_unsafe_listener_type listener_from_three(streams_manager.test,
                                                                                    SimpleOrderKey(3));
        typename Impl::test_type::INTERNAL_unsafe_listener_type listener_from_three_to_five_not_inclusive(
            streams_manager.test, SimpleOrderKey(3), SimpleOrderKey(5));

        ASSERT_FALSE(listener_all.HasData());
        ASSERT_FALSE(listener_all.ReachedEnd());

        ASSERT_FALSE(listener_from_three.HasData());
        ASSERT_FALSE(listener_from_three.ReachedEnd());

        ASSERT_FALSE(listener_from_three_to_five_not_inclusive.HasData());
        ASSERT_FALSE(listener_from_three_to_five_not_inclusive.ReachedEnd());

        publisher.Push(SimpleEntry(1, "one"));
        publisher.Push(SimpleEntry(2, "two"));

        ASSERT_TRUE(listener_all.HasData());
        ASSERT_FALSE(listener_all.ReachedEnd());
        listener_all.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(1, entry.ikey);
            EXPECT_EQ("one", entry.data);
        });
        listener_all.AdvanceToNextEntry();
        listener_all.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(2, entry.ikey);
            EXPECT_EQ("two", entry.data);
        });
        listener_all.AdvanceToNextEntry();
        ASSERT_FALSE(listener_all.HasData());
        ASSERT_FALSE(listener_from_three.HasData());
        ASSERT_FALSE(listener_from_three.ReachedEnd());
        ASSERT_FALSE(listener_from_three_to_five_not_inclusive.ReachedEnd());
        ASSERT_FALSE(listener_from_three_to_five_not_inclusive.HasData());

        publisher.Push(SimpleEntry(3, "three"));
        publisher.Push(SimpleEntry(4, "four"));

        ASSERT_TRUE(listener_all.HasData());
        ASSERT_FALSE(listener_all.ReachedEnd());
        listener_all.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(3, entry.ikey);
            EXPECT_EQ("three", entry.data);
        });
        listener_all.AdvanceToNextEntry();
        listener_all.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(4, entry.ikey);
            EXPECT_EQ("four", entry.data);
        });
        listener_all.AdvanceToNextEntry();
        ASSERT_FALSE(listener_all.HasData());

        ASSERT_TRUE(listener_from_three.HasData());
        ASSERT_FALSE(listener_from_three.ReachedEnd());
        listener_from_three.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(3, entry.ikey);
            EXPECT_EQ("three", entry.data);
        });
        listener_from_three.AdvanceToNextEntry();
        listener_from_three.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(4, entry.ikey);
            EXPECT_EQ("four", entry.data);
        });
        listener_from_three.AdvanceToNextEntry();
        ASSERT_FALSE(listener_from_three.HasData());

        ASSERT_TRUE(listener_from_three_to_five_not_inclusive.HasData());
        ASSERT_FALSE(listener_from_three_to_five_not_inclusive.ReachedEnd());
        listener_from_three_to_five_not_inclusive.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(3, entry.ikey);
            EXPECT_EQ("three", entry.data);
        });
        listener_from_three_to_five_not_inclusive.AdvanceToNextEntry();
        listener_from_three_to_five_not_inclusive.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(4, entry.ikey);
            EXPECT_EQ("four", entry.data);
        });
        listener_from_three_to_five_not_inclusive.AdvanceToNextEntry();
        ASSERT_FALSE(listener_from_three_to_five_not_inclusive.HasData());

        publisher.Push(SimpleEntry(5, "five"));
        publisher.Push(SimpleEntry(6, "six"));
        publisher.Push(SimpleEntry(7, "seven"));

        ASSERT_TRUE(listener_all.HasData());
        ASSERT_FALSE(listener_all.ReachedEnd());
        listener_all.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(5, entry.ikey);
            EXPECT_EQ("five", entry.data);
        });
        listener_all.AdvanceToNextEntry();
        listener_all.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(6, entry.ikey);
            EXPECT_EQ("six", entry.data);
        });
        listener_all.AdvanceToNextEntry();
        listener_all.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(7, entry.ikey);
            EXPECT_EQ(("seven"), entry.data);
        });
        listener_all.AdvanceToNextEntry();
        ASSERT_FALSE(listener_all.HasData());

        ASSERT_TRUE(listener_from_three.HasData());
        ASSERT_FALSE(listener_from_three.ReachedEnd());
        listener_from_three.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(5, entry.ikey);
            EXPECT_EQ(("five"), entry.data);
        });
        listener_from_three.AdvanceToNextEntry();
        listener_from_three.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(6, entry.ikey);
            EXPECT_EQ(("six"), entry.data);
        });
        listener_from_three.AdvanceToNextEntry();
        listener_from_three.ProcessEntrySync([](const SimpleEntry& entry) {
            EXPECT_EQ(7, entry.ikey);
            EXPECT_EQ(("seven"), entry.data);
        });
        listener_from_three.AdvanceToNextEntry();
        ASSERT_FALSE(listener_from_three.HasData());

        ASSERT_FALSE(listener_from_three_to_five_not_inclusive.HasData());
        ASSERT_TRUE(listener_from_three_to_five_not_inclusive.ReachedEnd());

        std::string golden =
            "[0]:{1,'one'},[1]:{2,'two'},[2]:{3,'three'},[3]:{4,'four'},[4]:{5,'five'},[5]:{6,'six'},[6]:{7,'seven'"
            "}";
        ASSERT_EQ(golden, stats.data);
    }
}

TEST(StreamManagerSmokeTest, DataInjected) {
    TAILPRODUCE_STATIC_FRAMEWORK_BEGIN(Impl, MockStreamManager<MockDataStorage>);
    TAILPRODUCE_STREAM(foo, SimpleEntry, SimpleOrderKey);
    TAILPRODUCE_PUBLISHER(foo);
    TAILPRODUCE_STATIC_FRAMEWORK_END();

    MockDataStorage storage;

    {
        // Mimic the 1st run with the command line flag set to initialize the stream in the storage.
        Impl streams_manager(storage, StreamManagerParams().CreateStream("foo", SimpleOrderKey(100)));
    }

    // Add one data entry.
    storage.Set("d:foo:0000000042:0000000000", bytes("{\"value0\":{\"ikey\":42,\"data\":\"Yay!\"}}"));

    {
        // Mimic the consecutive run(s) that rely on the fact that the stream exists.
        Impl streams_manager(storage, StreamManagerParams());
        Stats stats;
        EXPECT_EQ(0, stats.count);
        auto foo_listener_existence_scope = streams_manager.new_scoped_foo_listener(Aggregator(stats));
        EXPECT_EQ(1, stats.count);
        EXPECT_EQ("[0]:{42,'Yay!'}", stats.data);
    }
}
