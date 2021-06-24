#include <atomic>
#include <chrono>
#include <map>
#include <random>
#include <sstream>
#include <thread>
#include <type_traits>

#include "ivi/ivi-client-mgr.h"
#include "ivi/ivi-config.h"
#include "ivi/ivi-model.h"
#include "ivi/ivi-types.h"
#include "ivi/ivi-util.h"

#include "ivi/generated/common/common.grpc.pb.h"
#include "ivi/generated/api/item/definition.grpc.pb.h"
#include "ivi/generated/api/item/rpc.grpc.pb.h"
#include "ivi/generated/api/itemtype/definition.grpc.pb.h"
#include "ivi/generated/api/itemtype/rpc.grpc.pb.h"
#include "ivi/generated/api/order/rpc.grpc.pb.h"
#include "ivi/generated/api/payment/rpc.grpc.pb.h"
#include "ivi/generated/api/player/rpc.grpc.pb.h"
#include "ivi/generated/streams/common.grpc.pb.h"
#include "ivi/generated/streams/item/stream.grpc.pb.h"
#include "ivi/generated/streams/itemtype/stream.grpc.pb.h"
#include "ivi/generated/streams/order/stream.grpc.pb.h"
#include "ivi/generated/streams/player/stream.grpc.pb.h"

#include "grpcpp/grpcpp.h"
#include "gtest/gtest.h"

/*
* These IVI SDK unit tests are meant to verify integrity of the RPC marshalers and parsers
* for the various IVI types.  These tests are *not* meant to demonstrate fine-grained 
* IVI Engine semantics, see the ivi-sdk-example and documentation for that.
* 
* Also note, at the time of this writing, GTest does not support multithreaded tests on
* Win32, so all test data is marshaled for testing by the GTest thread on the Fake*Service
* instance.  This is particularly relevant for the Stream RPC tests.
*/

using namespace ivi;

std::mt19937& RandomEng()
{
    static std::random_device randDev;
    static std::mt19937 randEng(randDev());
    return randEng;
}

template<typename TInt = uint32_t>
TInt RandomInt()
{
    return static_cast<TInt>(RandomEng()());
}

template<>
int64_t RandomInt<int64_t>()
{
    int64_t retVal;
    uint32_t* const casted(reinterpret_cast<uint32_t*>(&retVal));
    casted[0] = RandomInt();
    casted[1] = RandomInt();
    return retVal;
}

uint32_t RandomCount()
{
    return RandomInt<uint32_t>() % 20 + 1;
}

bool RandomBool()
{
    return static_cast<bool>(RandomInt() % 2 == 0);
}

template<class RealType = double> 
RealType RandomFloat(RealType min = 0.f, RealType max = 1.f)
{
    return std::uniform_real_distribution<RealType>(min, max)(RandomEng());
}

template<class RealType = double> 
string RandomFloatString(RealType min = 0.f, RealType max = 1.f)
{
    return std::to_string(RandomFloat<RealType>(min, max));
}

string RandomString(uint32_t len)
{
    const char alphanumerics[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    std::string retVal;

    retVal.reserve(len);
    for (uint32_t i = 0; i < len; i += sizeof(uint32_t))
    {
        const uint32_t val(RandomInt());
        const std::uint8_t* valu8(reinterpret_cast<const std::uint8_t*>(&val));
        for (uint32_t j = i; j < i + sizeof(val) && j < len; ++j)
        {
            retVal += alphanumerics[valu8[j % sizeof(val)] % (sizeof(alphanumerics) - 1)];
        }
    }

    return retVal;
}

list<string> RandomStringList(int32_t strLen, int32_t maxListLen)
{
    list<string> retVal;
    const int count = RandomInt() % maxListLen;
    for (int i = 0; i < maxListLen; ++i)
        retVal.push_back(RandomString(strLen));
    return retVal;
}

template<typename TMap>
typename TMap::key_type RandomKey(const TMap& aMap)
{
    const uint32_t idx = RandomInt() % aMap.size();
    auto it(aMap.begin());
    for (uint32_t i = 0; i < idx; ++i, ++it);
    return it->first;
}

time_t Now()
{
    return std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
}

// Simple spin-wait for unit testing... obviously don't do this in production
template<typename Condition>
void SpinWait(Condition&& func)
{
    while (func())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

// Temporarily hook the IVI Logger for the below test so we don't output expected messages to the unit test output
template<LogLevel TMinReportingLevel>
struct ErrorTestLogFilter
{
#if IVI_LOGGING_LEVEL > 0

    static LogFunc& GetOrigImpl()
    {
        static LogFunc OrigLogImpl;
        return OrigLogImpl;
    }

    using LogCountMap = std::map<int, int>;
    static LogCountMap& GetLogCounter()
    {
        static LogCountMap countMap;
        return countMap;
    }


    static void Logger(LogLevel logLevel, const string& msg)
    {
        ASSERT_GE(logLevel, TMinReportingLevel);
        ++GetLogCounter()[static_cast<int>(logLevel)];
        if (logLevel > TMinReportingLevel)
            return;
        GetOrigImpl()(logLevel, msg);    // only critical messages get reported for the test
    }

    ErrorTestLogFilter()
    {
        GetLogCounter().clear();
        GetOrigImpl() = IVILogImpl;
        IVILogImpl = &Logger;
    }

    ~ErrorTestLogFilter()
    {
        IVILogImpl = GetOrigImpl();
    }
#endif // IVI_LOGGING_LEVEL > 0
};

static const string EnvironmentId = RandomString(12);
static const string ApiKey = RandomString(32);

static IVIStreamCallbacks NoStreamCallbacks;

template<class TService, IVIStreamCallbacks* TCallbacks = &NoStreamCallbacks, LogLevel TMinReportingLevel = LogLevel::CRITICAL>
class ClientTest : public ::testing::Test
{
protected:

    using Base = ClientTest<TService>;

    ClientTest() = default;

    void SetUp() override
    {
        constexpr const uint32_t portMin = 1 << 13;
        constexpr const uint32_t portMax = 1 << 15;
        static uint32_t port = RandomInt() % (portMax - portMin);
        string host = "localhost:" + std::to_string(++port);

        // Setup server
        grpc::ServerBuilder builder;
        builder.AddListeningPort(host, grpc::InsecureServerCredentials());
        builder.RegisterService(&m_service);
        m_server = builder.BuildAndStart();

        IVIConfigurationPtr config(IVIConfiguration::DefaultConfiguration(EnvironmentId, ApiKey, host));
        IVIConnectionPtr connection(make_shared<IVIConnection>(
            IVIConnection{ grpc::CreateChannel(host, grpc::InsecureChannelCredentials()),
                            make_shared<grpc::CompletionQueue>(),
                            make_shared<grpc::CompletionQueue>()
            }));

        m_syncManager.reset(new IVIClientManagerSync(config, connection));
        m_asyncManager.reset(new IVIClientManagerAsync(config, connection, *TCallbacks));
    }

    void TearDown() override
    {
        m_asyncManager.reset(nullptr);
        m_syncManager.reset(nullptr);
        m_server->Shutdown();
    }

    unique_ptr<grpc::Server>            m_server;

public:

    unique_ptr<IVIClientManagerSync>    m_syncManager;

    unique_ptr<IVIClientManagerAsync>   m_asyncManager;

    TService                            m_service;

    using LogFilter                     = ErrorTestLogFilter<TMinReportingLevel>;

    LogFilter                           m_logFilter;

    // Template for the boilerplate code for sync & async call testing
    template<typename TData, typename TResultChecker, typename TSyncCaller, typename TAsyncCaller>
    void UnaryTest(TResultChecker&& checker, TSyncCaller&& syncCall, TAsyncCaller&& asyncCall)
    {
        {
            TData data;
            auto syncResult(syncCall(data));
            checker(data, syncResult);
        }

        {
            TData data;
            bool resultReceived = false;
            asyncCall(data,
                [&](typename std::add_lvalue_reference<typename std::add_const<decltype(syncCall(data))>::type >::type asyncResult)
                {
                    checker(data, asyncResult);
                    resultReceived = true;
                });

            while (!resultReceived)
            {
                ASSERT_TRUE(m_asyncManager->Poll());
            }
        }
    }
};

string GenerateJsonString()
{
    std::ostringstream json;
    if (RandomBool())
    {
        json << "{\"" << RandomString(8) << "\":\"" << RandomString(16) << "\"}";
    }
    return json.str();
}

IVIMetadata GenerateMetadata()
{
    proto::common::Metadata retVal;
    retVal.set_name(RandomString(30));
    retVal.set_description(RandomString(30));
    retVal.set_image(RandomString(30));
    *retVal.mutable_properties() = JsonStringToGoogleStruct(GenerateJsonString());
    return IVIMetadata::FromProto(retVal);
}

IVIItem GenerateItem(
    int64_t dGoodsId,
    const string& sideChainAccount,
    int32_t serialNumber,
    const string& metadataUri,
    const string& trackingId,
    ItemState state)
{
    return
    {
        RandomString(12),
        RandomString(18),
        dGoodsId,
        RandomString(20),
        RandomString(22),
        sideChainAccount,
        serialNumber,
        RandomString(24),
        metadataUri,
        trackingId,
        GenerateMetadata(),
        Now() - 10000,
        Now(),
        state,
    };
}

IVIItem GenerateItem()
{
    return GenerateItem(0, RandomString(26), 0, RandomString(28), RandomString(30), ItemState::PENDING_ISSUED);
}

void CheckEq(const IVIMetadata& lhs, const IVIMetadata& rhs)
{
    ASSERT_NE(&lhs, &rhs);
    ASSERT_EQ(lhs.name, rhs.name);
    ASSERT_EQ(lhs.description, rhs.description);
    ASSERT_EQ(lhs.image, rhs.image);
    ASSERT_EQ(lhs.properties, rhs.properties);
}

void CheckEq(const IVIItem& lhs, const IVIItem& rhs)
{
    ASSERT_NE(&lhs, &rhs);
    ASSERT_EQ(lhs.gameInventoryId, rhs.gameInventoryId);
    ASSERT_EQ(lhs.gameItemTypeId, rhs.gameItemTypeId);
    ASSERT_EQ(lhs.dgoodsId, rhs.dgoodsId);
    ASSERT_EQ(lhs.itemName, rhs.itemName);
    ASSERT_EQ(lhs.playerId, rhs.playerId);
    ASSERT_EQ(lhs.ownerSidechainAccount, rhs.ownerSidechainAccount);
    ASSERT_EQ(lhs.serialNumber, rhs.serialNumber);
    ASSERT_EQ(lhs.currencyBase, rhs.currencyBase);
    ASSERT_EQ(lhs.metadataUri, rhs.metadataUri);
    ASSERT_EQ(lhs.trackingId, rhs.trackingId);
    ASSERT_EQ(lhs.itemState, rhs.itemState);
    ASSERT_EQ(lhs.createdTimestamp, rhs.createdTimestamp);
    ASSERT_EQ(lhs.updatedTimestamp, rhs.updatedTimestamp);
    CheckEq(lhs.metadata, rhs.metadata);
}

TEST(DefaultConfiguration, Initialization)
{
	IVIConfigurationPtr config(IVIConfiguration::DefaultConfiguration("foo","bar"));
	EXPECT_EQ(config->environmentId, "foo");
	EXPECT_EQ(config->apiKey, "bar");
	EXPECT_EQ(config->host, IVIConfiguration::DefaultHost());
	EXPECT_EQ(config->autoconfirmStreamUpdates, true);
}

::grpc::Status AnError(::grpc::StatusCode code)
{
    return ::grpc::Status{ code, "an error occurred" };
}

::grpc::Status AnError()
{
    return AnError(::grpc::StatusCode::UNKNOWN);
}

/*
* Testing note: Non-success responses are not universally tested because this is done in
* a function template that is universally used and bypasses parsing, so one test is 
* sufficient for that coverage.
* The only current exception to this rule is IVIItemTypeClient::GetItemType because
* it wraps and re-parses a different RPC on the client side.
*/

class FakeItemService : public rpc::api::item::ItemService::Service
{
public:
    using ItemMap = std::map<string, IVIItem>;
protected:
    static ItemMap& SomeItemsNC()
    {
        struct Items
        {
            ItemMap items;
            Items()
            {
                const uint32_t count = RandomCount();
                for (uint32_t i = 0; i < count; ++i)
                {
                    IVIItem item(GenerateItem());
                    items[item.gameInventoryId] = move(item);
                }
            }
        };
        static Items randomItems;
        return randomItems.items;
    }
public:
    static const ItemMap& SomeItems()
    {
        return SomeItemsNC();
    }

    string lastTrackingId;

    proto::api::item::IssueItemRequest lastIssueItemRequest;
    ::grpc::Status IssueItem(::grpc::ServerContext* context, const proto::api::item::IssueItemRequest* request, proto::api::item::IssueItemStartedResponse* response) override
    {
        lastIssueItemRequest = *request;

        if (request->game_inventory_id().size() == 0)
        {
            return AnError();
        }
        lastTrackingId = RandomString(32);
        response->set_tracking_id(lastTrackingId);
        response->set_item_state(ECast(ItemState::PENDING_ISSUED));
        return ::grpc::Status::OK;
    }

    proto::api::item::TransferItemRequest lastTransferItemRequest;
    ::grpc::Status TransferItem(::grpc::ServerContext* context, const proto::api::item::TransferItemRequest* request, proto::api::item::TransferItemStartedResponse* response) override
    {
        lastTransferItemRequest = *request;
        lastTrackingId = RandomString(16);
        response->set_tracking_id(lastTrackingId);
        response->set_item_state(ECast(ItemState::PENDING_TRANSFERRED));
        return ::grpc::Status::OK;
    }

    proto::api::item::BurnItemRequest lastBurnItemRequest;
    ::grpc::Status BurnItem(::grpc::ServerContext* context, const proto::api::item::BurnItemRequest* request, ::ivi::proto::api::item::BurnItemStartedResponse* response) override
    {
        lastBurnItemRequest = *request;
        lastTrackingId = RandomString(18);
        response->set_tracking_id(lastTrackingId);
        response->set_item_state(ECast(ItemState::PENDING_BURNED));
        return ::grpc::Status::OK;
    }

    proto::api::item::GetItemRequest lastGetItemRequest;
    ::grpc::Status GetItem(::grpc::ServerContext* context, const proto::api::item::GetItemRequest* request, proto::api::item::Item* response) override
    {
        lastGetItemRequest = *request;
        auto item(SomeItems().find(request->game_inventory_id()));
        if (item != SomeItems().end())
        {
            *response = item->second.ToProto(); // if this throws, something is wrong with the test itself
            return ::grpc::Status::OK;
        }
        return AnError(::grpc::StatusCode::NOT_FOUND);
    }

    proto::api::item::GetItemsRequest lastGetItemsRequest;
    ::grpc::Status GetItems(::grpc::ServerContext* context, const proto::api::item::GetItemsRequest* request, ::ivi::proto::api::item::Items* response) override
    {
        lastGetItemsRequest = *request;
        transform(SomeItems().begin(), SomeItems().end(), google::protobuf::RepeatedPtrFieldBackInserter(response->mutable_items()),
            [](const ItemMap::value_type& itemPair) { return itemPair.second.ToProto();  });
        return ::grpc::Status::OK;
    }

    proto::api::item::UpdateItemMetadataRequest lastUpdateItemMetadataRequest;
    ::grpc::Status UpdateItemMetadata(::grpc::ServerContext* context, const ::ivi::proto::api::item::UpdateItemMetadataRequest* request, ::ivi::proto::api::item::UpdateItemMetadataResponse* response) override
    {
        lastUpdateItemMetadataRequest = *request;

        for (auto it = request->update_items().begin(); it != request->update_items().end(); ++it)
        {
            auto itFind(SomeItemsNC().find(it->game_inventory_id()));
            if (itFind == SomeItemsNC().end())
            {
                return AnError(::grpc::StatusCode::NOT_FOUND);
            }
            else
            {
                itFind->second.metadata = IVIMetadata::FromProto(it->metadata());
            }
        }

        return ::grpc::Status::OK;
    }
};

using ItemClientTest = ClientTest<FakeItemService>;

TEST_F(ItemClientTest, IssueItem)
{
    struct RPCTestData
    {
        IVIItem item{ GenerateItem() };
        BigDecimal amountPaid{ RandomFloatString(0.f, 100.f) };
        string currency{ RandomString(2) }
            , storeId{ RandomString(8) }
            , orderId{ RandomString(64) };
    };

    auto checkResultSuccess = [&](const RPCTestData& data, const IVIResultItemStateChange& result)
    {
        const proto::api::item::IssueItemRequest& request(m_service.lastIssueItemRequest);
        ASSERT_TRUE(result.Success());
        ASSERT_EQ(result.Payload().gameInventoryId, data.item.gameInventoryId);
        ASSERT_EQ(result.Payload().itemState, ItemState::PENDING_ISSUED);
        ASSERT_EQ(result.Payload().trackingId, m_service.lastTrackingId);
        ASSERT_EQ(request.game_inventory_id(), data.item.gameInventoryId);
        ASSERT_EQ(request.player_id(), data.item.playerId);
        ASSERT_EQ(request.item_name(), data.item.itemName);
        ASSERT_EQ(request.game_item_type_id(), data.item.gameItemTypeId);
        ASSERT_EQ(request.amount_paid(), data.amountPaid);
        ASSERT_EQ(request.currency(), data.currency);
        ASSERT_EQ(request.store_id(), data.storeId);
        ASSERT_EQ(request.order_id(), data.orderId);
        ASSERT_EQ(request.request_ip(), "127.0.0.1");
        CheckEq(IVIMetadata::FromProto(request.metadata()), data.item.metadata);
    };

    auto syncCaller = [&](const RPCTestData& data)
    {
        return m_syncManager->ItemClient().IssueItem(
            data.item.gameInventoryId, data.item.playerId, data.item.itemName, data.item.gameItemTypeId, data.amountPaid, data.currency, data.item.metadata, data.storeId, data.orderId, "127.0.0.1");
    };

    auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultItemStateChange&)>& callback)
    {
        m_asyncManager->ItemClient().IssueItem(
            data.item.gameInventoryId, data.item.playerId, data.item.itemName, data.item.gameItemTypeId, data.amountPaid, data.currency, data.item.metadata, data.storeId, data.orderId, "127.0.0.1", callback);
    };

    ClientTest::template UnaryTest<RPCTestData>(checkResultSuccess, syncCaller, asyncCaller);

    struct BadRPCTestData : public RPCTestData { BadRPCTestData() { item.gameInventoryId = ""; } };
    auto checkResultFail = [&](const RPCTestData& data, const IVIResultItemStateChange& result)
    {
        const proto::api::item::TransferItemRequest& request(m_service.lastTransferItemRequest);
        ASSERT_FALSE(result.Success());
        ASSERT_EQ(result.Payload().gameInventoryId, "");
    };

    ClientTest::template UnaryTest<BadRPCTestData>(checkResultFail, syncCaller, asyncCaller);
}

TEST_F(ItemClientTest, TransferItem)
{
    struct RPCTestData
    {
        string gameInventoryId{ RandomString(8) }
             , sourcePlayerId{ RandomString(12) }
             , destPlayerId{ RandomString(15) }
             , storeId{ RandomString(4) };
    };

    auto checkResultSuccess = [&](const RPCTestData& data, const IVIResultItemStateChange& result)
    {
        const proto::api::item::TransferItemRequest& request(m_service.lastTransferItemRequest);
        ASSERT_TRUE(result.Success());
        ASSERT_EQ(result.Payload().gameInventoryId, data.gameInventoryId);
        ASSERT_EQ(result.Payload().itemState, ItemState::PENDING_TRANSFERRED);
        ASSERT_EQ(result.Payload().trackingId, m_service.lastTrackingId);
        ASSERT_EQ(request.game_item_inventory_id(), data.gameInventoryId);
        ASSERT_EQ(request.source_player_id(), data.sourcePlayerId);
        ASSERT_EQ(request.destination_player_id(), data.destPlayerId);
        ASSERT_EQ(request.store_id(), data.storeId);
    };

    auto syncCaller = [&](const RPCTestData& data)
    {
        return m_syncManager->ItemClient().TransferItem(
            data.gameInventoryId, data.sourcePlayerId, data.destPlayerId, data.storeId);
    };

    auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultItemStateChange&)>& callback)
    {
        return m_asyncManager->ItemClient().TransferItem(
            data.gameInventoryId, data.sourcePlayerId, data.destPlayerId, data.storeId, callback);
    };

    ClientTest::template UnaryTest<RPCTestData>(checkResultSuccess, syncCaller, asyncCaller);
}

TEST_F(ItemClientTest, BurnItem)
{
    struct RPCTestData
    {
        string gameInventoryId{ RandomString(30) };
    };

    auto checkResultSuccess = [&](const RPCTestData& data, const IVIResultItemStateChange& result)
    {
        const proto::api::item::BurnItemRequest& request(m_service.lastBurnItemRequest);
        ASSERT_TRUE(result.Success());
        ASSERT_EQ(result.Payload().gameInventoryId, data.gameInventoryId);
        ASSERT_EQ(result.Payload().itemState, ItemState::PENDING_BURNED);
        ASSERT_EQ(result.Payload().trackingId, m_service.lastTrackingId);
        ASSERT_EQ(request.game_item_inventory_id(), data.gameInventoryId);
    };

    auto syncCaller = [&](const RPCTestData& data)
    {
        return m_syncManager->ItemClient().BurnItem(
            data.gameInventoryId);
    };

    auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultItemStateChange&)>& callback)
    {
        return m_asyncManager->ItemClient().BurnItem(
            data.gameInventoryId, callback);
    };

    ClientTest::template UnaryTest<RPCTestData>(checkResultSuccess, syncCaller, asyncCaller);
}

TEST_F(ItemClientTest, GetItem)
{
    for(int i = 0; i < FakeItemService::SomeItems().size(); ++i)
    {
        struct RPCTestData
        {
            string gameInventoryId{ RandomKey(FakeItemService::SomeItems()) };
        };

        auto checkResultSuccess = [&](const RPCTestData& data, const IVIResultItem& result)
        {
            const proto::api::item::GetItemRequest& request(m_service.lastGetItemRequest);
            ASSERT_TRUE(result.Success());
            ASSERT_TRUE(request.history()); // matches caller param below
            CheckEq(result.Payload(), FakeItemService::SomeItems().at(data.gameInventoryId));
        };

        auto syncCaller = [&](const RPCTestData& data)
        {
            return m_syncManager->ItemClient().GetItem(data.gameInventoryId, true);
        };

        auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultItem&)>& callback)
        {
            return m_asyncManager->ItemClient().GetItem(data.gameInventoryId, true, callback);
        };

        ClientTest::template UnaryTest<RPCTestData>(checkResultSuccess, syncCaller, asyncCaller);

        struct BadRPCTestData : RPCTestData
        {
            BadRPCTestData() { gameInventoryId = RandomString(23); }
        };

        auto checkResultFailure = [](const BadRPCTestData& data, const IVIResultItem& result)
        {
            ASSERT_FALSE(result.Success());
            ASSERT_EQ(result.Status(), IVIResultStatus::NOT_FOUND);
        };

        ClientTest::template UnaryTest<BadRPCTestData>(checkResultFailure, syncCaller, asyncCaller);
    }
}

TEST_F(ItemClientTest, GetItems)
{
    struct RPCTestData
    {
        time_t timestamp = Now() - RandomInt() % 100000;
        int32_t pageSize = RandomInt() % 128;
        SortOrder sortOrder = static_cast<SortOrder>(RandomInt() % proto::common::sort::SortOrder_ARRAYSIZE);
        Finalized finalized = static_cast<Finalized>(RandomInt() % proto::common::finalization::Finalized_ARRAYSIZE);
    };

    auto checkResultSuccess = [&](const RPCTestData& data, const IVIResultItemList& result)
    {
        const proto::api::item::GetItemsRequest& request(m_service.lastGetItemsRequest);
        ASSERT_TRUE(result.Success());
        ASSERT_EQ(data.timestamp, request.created_timestamp());
        ASSERT_EQ(data.pageSize, request.page_size());
        ASSERT_EQ(data.sortOrder, ECast(request.sort_order()));
        ASSERT_EQ(data.finalized, ECast(request.finalized()));
        ASSERT_TRUE(result.Payload().size() > 0);
        ASSERT_EQ(result.Payload().size(), FakeItemService::SomeItems().size());
        std::for_each(result.Payload().begin(), result.Payload().end(),
            [](const IVIItem& item)
            { CheckEq(FakeItemService::SomeItems().at(item.gameInventoryId), item); });
    };

    auto syncCaller = [&](const RPCTestData& data)
    {
        return m_syncManager->ItemClient().GetItems(
            data.timestamp, data.pageSize, data.sortOrder, data.finalized);
    };

    auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultItemList&)>& callback)
    {
        m_asyncManager->ItemClient().GetItems(
            data.timestamp, data.pageSize, data.sortOrder, data.finalized, callback);
    };

    ClientTest::template UnaryTest<RPCTestData>(checkResultSuccess, syncCaller, asyncCaller);
}

TEST_F(ItemClientTest, UpdateItemMetadata)
{
    for(int i = 0; i < FakeItemService::SomeItems().size(); ++i)
    {
        struct RPCTestData
        {
            string gameInventoryId{ RandomKey(FakeItemService::SomeItems()) };
            IVIMetadata metadata{ GenerateMetadata() };
        };

        auto checkResultSuccess = [&](const RPCTestData& data, const IVIResult& result)
        {
            const proto::api::item::UpdateItemMetadataRequest& request(m_service.lastUpdateItemMetadataRequest);
            ASSERT_TRUE(result.Success());
            ASSERT_EQ(request.update_items_size(), 1);
            ASSERT_EQ(data.gameInventoryId, request.update_items()[0].game_inventory_id());
            CheckEq(data.metadata, IVIMetadata::FromProto(request.update_items()[0].metadata()));
        };

        auto syncCaller = [&](const RPCTestData& data)
        {
            return m_syncManager->ItemClient().UpdateItemMetadata(data.gameInventoryId, data.metadata);
        };

        auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResult&)>& callback)
        {
            m_asyncManager->ItemClient().UpdateItemMetadata(data.gameInventoryId, data.metadata, callback);
        };

        ClientTest::template UnaryTest<RPCTestData>(checkResultSuccess, syncCaller, asyncCaller);

        struct BadRPCTestData : RPCTestData
        {
            BadRPCTestData() { gameInventoryId = RandomString(128); }
        };

        auto checkResultFailed = [](const RPCTestData& data, const IVIResult& result)
        {
            ASSERT_FALSE(result.Success());
            ASSERT_EQ(result.Status(), IVIResultStatus::NOT_FOUND);
        };

        ClientTest::template UnaryTest<BadRPCTestData>(checkResultFailed, syncCaller, asyncCaller);
    }
}

TEST_F(ItemClientTest, UpdateItemMetadataList)
{
    struct RPCTestData
    {
        IVIMetadataUpdateList metadataUpdates;
        RPCTestData()
        {
            transform(FakeItemService::SomeItems().begin(), FakeItemService::SomeItems().end(), back_inserter(metadataUpdates),
                [&](const FakeItemService::ItemMap::value_type& item)
                { return IVIMetadataUpdate{ item.first, GenerateMetadata() }; });
        }
    };

    auto checkResultSuccess = [&](const RPCTestData& data, const IVIResult& result)
    {
        const proto::api::item::UpdateItemMetadataRequest& request(m_service.lastUpdateItemMetadataRequest);
        ASSERT_TRUE(result.Success());
        ASSERT_EQ(request.update_items_size(), data.metadataUpdates.size());

        std::for_each(data.metadataUpdates.begin(), data.metadataUpdates.end(),
            [](const IVIMetadataUpdate& update)
            { 
                auto it = FakeItemService::SomeItems().find(update.gameInventoryId);
                ASSERT_NE(it, FakeItemService::SomeItems().end());
                CheckEq(update.metadata, it->second.metadata);
            });
    };

    auto syncCaller = [&](const RPCTestData& data)
    {
        return m_syncManager->ItemClient().UpdateItemMetadata(data.metadataUpdates);
    };

    auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResult&)>& callback)
    {
        m_asyncManager->ItemClient().UpdateItemMetadata(data.metadataUpdates, callback);
    };

    ClientTest::template UnaryTest<RPCTestData>(checkResultSuccess, syncCaller, asyncCaller);
}

IVIItemType GenerateItemType()
{
    uint32_t maxSupply = RandomInt() % (1024 * 1024);
    uint32_t currentSupply = maxSupply % 1024;
    uint32_t issuedSupply = maxSupply - currentSupply;
    return
    {
        RandomString(16),
        static_cast<int32_t>(maxSupply),
        static_cast<int32_t>(currentSupply),
        static_cast<int32_t>(issuedSupply),
        RandomString(32),
        RandomInt<int32_t>(),
        RandomString(12),
        RandomString(32),
        RandomString(64),
        RandomStringList(14, 16),
        RandomString(18),
        GenerateMetadata(),
        Now() - 20000,
        Now(),
        static_cast<ItemTypeState>(RandomInt() % proto::common::itemtype::ItemTypeState_ARRAYSIZE),
        RandomBool(),
        RandomBool(),
        RandomBool(),
        RandomBool(),
        RandomBool()
    };
}

void CheckEq(const IVIItemType& lhs, const IVIItemType& rhs)
{
    ASSERT_NE(&lhs, &rhs);
    ASSERT_EQ(lhs.gameItemTypeId, rhs.gameItemTypeId);
    ASSERT_EQ(lhs.maxSupply, rhs.maxSupply);
    ASSERT_EQ(lhs.currentSupply, rhs.currentSupply);
    ASSERT_EQ(lhs.issuedSupply, rhs.issuedSupply);
    ASSERT_EQ(lhs.issuer, rhs.issuer);
    ASSERT_EQ(lhs.issueTimeSpan, rhs.issueTimeSpan);
    ASSERT_EQ(lhs.category, rhs.category);
    ASSERT_EQ(lhs.tokenName, rhs.tokenName);
    ASSERT_EQ(lhs.baseUri, rhs.baseUri);
    ASSERT_EQ(lhs.agreementIds, rhs.agreementIds);
    ASSERT_EQ(lhs.trackingId, rhs.trackingId);
    CheckEq(lhs.metadata, rhs.metadata);
    ASSERT_EQ(lhs.createdTimestamp, rhs.createdTimestamp);
    ASSERT_EQ(lhs.updatedTimestamp, rhs.updatedTimestamp);
    ASSERT_EQ(lhs.itemTypeState, rhs.itemTypeState);
    ASSERT_EQ(lhs.fungible, rhs.fungible);
    ASSERT_EQ(lhs.burnable, rhs.burnable);
    ASSERT_EQ(lhs.transferable, rhs.transferable);
    ASSERT_EQ(lhs.finalized, rhs.finalized);
    ASSERT_EQ(lhs.sellable, rhs.sellable);
}

class FakeItemTypeService : public rpc::api::itemtype::ItemTypeService::Service
{
public:
    using ItemTypeMap = std::map<string, IVIItemType>;

protected:
    static ItemTypeMap& SomeItemTypesNC()
    {
        struct ItemTypes
        {
            ItemTypeMap itemTypes;
            ItemTypes()
            {
                const uint32_t count = RandomCount();
                for (uint32_t i = 0; i < count; ++i)
                {
                    IVIItemType itemType(GenerateItemType());
                    itemTypes[itemType.gameItemTypeId] = move(itemType);
                }
            }
        };

        static ItemTypes randomItemTypes;
        return randomItemTypes.itemTypes;
    }

public:
    static const ItemTypeMap& SomeItemTypes()
    {
        return SomeItemTypesNC();
    }

    string lastTrackingId;

    proto::api::itemtype::CreateItemTypeRequest lastCreateItemTypeRequest;
    ::grpc::Status CreateItemType(::grpc::ServerContext* context, const ::ivi::proto::api::itemtype::CreateItemTypeRequest* request, ::ivi::proto::api::itemtype::CreateItemAsyncResponse* response) override
    {
        lastCreateItemTypeRequest = *request;
        response->set_game_item_type_id(request->game_item_type_id());
        lastTrackingId = RandomString(22);
        response->set_tracking_id(lastTrackingId);
        response->set_item_type_state(ECast(ItemTypeState::PENDING_CREATE));
        return ::grpc::Status::OK;
    }

    proto::api::itemtype::GetItemTypesRequest lastGetItemTypeRequest;
    ::grpc::Status GetItemTypes(::grpc::ServerContext* context, const ::ivi::proto::api::itemtype::GetItemTypesRequest* request, ::ivi::proto::api::itemtype::ItemTypes* response) override
    {
        lastGetItemTypeRequest = *request;
        for (auto it(request->game_item_type_ids().begin()); it != request->game_item_type_ids().end(); ++it)
        {
            auto findIt(SomeItemTypes().find(*it));
            if (findIt == SomeItemTypes().end())
            {
                return AnError(::grpc::StatusCode::NOT_FOUND);
            }
            *response->add_item_types() = findIt->second.ToProto();
        }

        return ::grpc::Status::OK;
    }
    
    proto::api::itemtype::FreezeItemTypeRequest lastFreezeItemTypeRequest;
    ::grpc::Status FreezeItemType(::grpc::ServerContext* context, const ::ivi::proto::api::itemtype::FreezeItemTypeRequest* request, ::ivi::proto::api::itemtype::FreezeItemTypeAsyncResponse* response) override
    {
        lastFreezeItemTypeRequest = *request;
        lastTrackingId = RandomString(16);
        response->set_tracking_id(lastTrackingId);
        response->set_item_type_state(ECast(ItemTypeState::PENDING_FREEZE));
        return ::grpc::Status::OK;
    }

    proto::api::itemtype::UpdateItemTypeMetadataPayload lastUpdateItemTypeMetadataPayload;
    ::grpc::Status UpdateItemTypeMetadata(::grpc::ServerContext* context, const ::ivi::proto::api::itemtype::UpdateItemTypeMetadataPayload* request, ::google::protobuf::Empty* response) override
    {
        lastUpdateItemTypeMetadataPayload = *request;
        SomeItemTypesNC().at(request->game_item_type_id()).metadata = IVIMetadata::FromProto(request->metadata());
        return ::grpc::Status::OK;
    }
};

using ItemTypeClientTest = ClientTest<FakeItemTypeService>;

TEST_F(ItemTypeClientTest, GetItemType)
{
    for(int i = 0; i < FakeItemTypeService::SomeItemTypes().size(); ++i)
    {
        struct RPCTestData
        {
            string gameItemTypeId{ RandomKey(FakeItemTypeService::SomeItemTypes()) };
        };

        auto checkResultSuccess = [&](const RPCTestData& data, const IVIResultItemType& result)
        {
            const proto::api::itemtype::GetItemTypesRequest& request(m_service.lastGetItemTypeRequest);
            ASSERT_TRUE(result.Success());
            ASSERT_EQ(1, request.game_item_type_ids().size());
            ASSERT_EQ(data.gameItemTypeId, request.game_item_type_ids()[0]);
            CheckEq(result.Payload(), FakeItemTypeService::SomeItemTypes().at(data.gameItemTypeId));
        };

        auto syncCaller = [&](const RPCTestData& data)
        {
            return m_syncManager->ItemTypeClient().GetItemType(data.gameItemTypeId);
        };

        auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultItemType&)>& callback)
        {
            m_asyncManager->ItemTypeClient().GetItemType(data.gameItemTypeId, callback);
        };

        ClientTest::template UnaryTest<RPCTestData>(checkResultSuccess, syncCaller, asyncCaller);

        struct BadRPCTestData : RPCTestData { BadRPCTestData() { gameItemTypeId = RandomString(4); } };
        auto checkResultFailure = [&](const RPCTestData& data, const IVIResultItemType& result)
        {
            const proto::api::itemtype::GetItemTypesRequest& request(m_service.lastGetItemTypeRequest);
            ASSERT_FALSE(result.Success());
            ASSERT_EQ(result.Status(), IVIResultStatus::NOT_FOUND);
        };

        ClientTest::template UnaryTest<BadRPCTestData>(checkResultFailure, syncCaller, asyncCaller);
    }
}

TEST_F(ItemTypeClientTest, GetItemTypes)
{
    struct RPCTestData
    {
        list<string> gameItemTypeIds;
        RPCTestData()
        { 
            const size_t count = RandomInt() % FakeItemTypeService::SomeItemTypes().size();
            for (size_t i = 0; i < count; ++i)
            {
                gameItemTypeIds.push_back(RandomKey(FakeItemTypeService::SomeItemTypes()));
            }
        }
    };

    auto checkResultSuccess = [&](const RPCTestData& data, const IVIResultItemTypeList& result)
    {
        const proto::api::itemtype::GetItemTypesRequest& request(m_service.lastGetItemTypeRequest);
        ASSERT_TRUE(result.Success());
        const list<string> requestedIds{ request.game_item_type_ids().begin(), request.game_item_type_ids().end() };
        ASSERT_EQ(data.gameItemTypeIds, requestedIds);
        std::for_each(result.Payload().begin(), result.Payload().end(),
            [&data](const IVIItemType& itemType)
            {
                ASSERT_NE(std::find(data.gameItemTypeIds.begin(), data.gameItemTypeIds.end(), itemType.gameItemTypeId), data.gameItemTypeIds.end());
                CheckEq(itemType, FakeItemTypeService::SomeItemTypes().at(itemType.gameItemTypeId));
            });
    };

    auto syncCaller = [&](const RPCTestData& data)
    {
        return m_syncManager->ItemTypeClient().GetItemTypes(data.gameItemTypeIds);
    };

    auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultItemTypeList&)>& callback)
    {
        m_asyncManager->ItemTypeClient().GetItemTypes(data.gameItemTypeIds, callback);
    };

    ClientTest::template UnaryTest<RPCTestData>(checkResultSuccess, syncCaller, asyncCaller);
}

TEST_F(ItemTypeClientTest, CreateItemType)
{
    struct RPCTestData
    {
        string gameItemTypeId{RandomString(10)},
               tokenName{RandomString(12)},
               category{RandomString(8)};
        int32_t maxSupply = RandomInt(),
                issueTimeSpan = RandomInt();
        bool burnable = RandomBool(),
             transferable = RandomBool(),
             sellable = RandomBool();
        UUIDList agreementIds{ RandomStringList(6, 100) };
        IVIMetadata metadata{ GenerateMetadata() };
    };

    auto checkResultSuccess = [&](const RPCTestData& data, const IVIResultItemTypeStateChange& result)
    {
        const proto::api::itemtype::CreateItemTypeRequest& request(m_service.lastCreateItemTypeRequest);
        ASSERT_TRUE(result.Success());
        ASSERT_EQ(data.gameItemTypeId, result.Payload().gameItemTypeId);
        ASSERT_EQ(result.Payload().trackingId, m_service.lastTrackingId);
        ASSERT_EQ(result.Payload().itemTypeState, ItemTypeState::PENDING_CREATE);

        ASSERT_EQ(request.game_item_type_id(), data.gameItemTypeId);
        ASSERT_EQ(request.token_name(), data.tokenName);
        ASSERT_EQ(request.category(), data.category);
        ASSERT_EQ(request.max_supply(), data.maxSupply);
        ASSERT_EQ(request.issue_time_span(), data.issueTimeSpan);
        ASSERT_EQ(request.burnable(), data.burnable);
        ASSERT_EQ(request.transferable(), data.transferable);
        ASSERT_EQ(request.sellable(), data.sellable);
        const UUIDList reqAgreementIds{ request.agreement_ids().begin(), request.agreement_ids().end() };
        ASSERT_EQ(reqAgreementIds, data.agreementIds);
        CheckEq(IVIMetadata::FromProto(request.metadata()), data.metadata);
    };

    auto syncCaller = [&](const RPCTestData& data)
    {
        return m_syncManager->ItemTypeClient().CreateItemType(
            data.gameItemTypeId, data.tokenName, data.category, data.maxSupply, data.issueTimeSpan, data.burnable, data.transferable, data.sellable, data.agreementIds, data.metadata);
    };

    auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultItemTypeStateChange&)>& callback)
    {
        m_asyncManager->ItemTypeClient().CreateItemType(
            data.gameItemTypeId, data.tokenName, data.category, data.maxSupply, data.issueTimeSpan, data.burnable, data.transferable, data.sellable, data.agreementIds, data.metadata, callback);
    };

    ClientTest::template UnaryTest<RPCTestData>(checkResultSuccess, syncCaller, asyncCaller);
}

TEST_F(ItemTypeClientTest, FreezeItemType)
{
    struct RPCTestData
    {
        string gameItemTypeId{RandomString(15)};
    };

    auto checkResultSuccess = [&](const RPCTestData& data, const IVIResultItemTypeStateChange& result)
    {
        const proto::api::itemtype::FreezeItemTypeRequest& request(m_service.lastFreezeItemTypeRequest);
        ASSERT_TRUE(result.Success());
        ASSERT_EQ(data.gameItemTypeId, result.Payload().gameItemTypeId);
        ASSERT_EQ(result.Payload().trackingId, m_service.lastTrackingId);
        ASSERT_EQ(result.Payload().itemTypeState, ItemTypeState::PENDING_FREEZE);
        ASSERT_EQ(request.game_item_type_id(), data.gameItemTypeId);
    };

    auto syncCaller = [&](const RPCTestData& data)
    {
        return m_syncManager->ItemTypeClient().FreezeItemType(data.gameItemTypeId);
    };

    auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultItemTypeStateChange&)>& callback)
    {
        m_asyncManager->ItemTypeClient().FreezeItemType(data.gameItemTypeId, callback);
    };

    ClientTest::template UnaryTest<RPCTestData>(checkResultSuccess, syncCaller, asyncCaller);
}

TEST_F(ItemTypeClientTest, UpdateItemTypeMetadata)
{
    for(int i = 0; i < FakeItemTypeService::SomeItemTypes().size(); ++i)
    {
        struct RPCTestData
        {
            string gameItemTypeId{ RandomKey(FakeItemTypeService::SomeItemTypes()) };
            IVIMetadata metadata{ GenerateMetadata() };
        };

        auto checkResultSuccess = [&](const RPCTestData& data, const IVIResult& result)
        {
            const proto::api::itemtype::UpdateItemTypeMetadataPayload& request(m_service.lastUpdateItemTypeMetadataPayload);
            ASSERT_TRUE(result.Success());
            ASSERT_EQ(request.game_item_type_id(), data.gameItemTypeId);
            CheckEq(FakeItemTypeService::SomeItemTypes().at(data.gameItemTypeId).metadata, data.metadata);
        };

        auto syncCaller = [&](const RPCTestData& data)
        {
            return m_syncManager->ItemTypeClient().UpdateItemTypeMetadata(data.gameItemTypeId, data.metadata);
        };

        auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResult&)>& callback)
        {
            m_asyncManager->ItemTypeClient().UpdateItemTypeMetadata(data.gameItemTypeId, data.metadata, callback);
        };

        ClientTest::template UnaryTest<RPCTestData>(checkResultSuccess, syncCaller, asyncCaller);
    }
}

void CheckEq(const IVIPlayer& lhs, const IVIPlayer& rhs)
{
    ASSERT_NE(&lhs, &rhs);
    ASSERT_EQ(lhs.playerId, rhs.playerId);
    ASSERT_EQ(lhs.email, rhs.email);
    ASSERT_EQ(lhs.displayName, rhs.displayName);
    ASSERT_EQ(lhs.sidechainAccountName, rhs.sidechainAccountName);
    ASSERT_EQ(lhs.trackingId, rhs.trackingId);
    ASSERT_EQ(lhs.playerState, rhs.playerState);
    ASSERT_EQ(lhs.createdTimestamp, rhs.createdTimestamp);
}

IVIPlayer GeneratePlayer()
{
    return
    {
        RandomString(23),
        RandomString(18),
        RandomString(44),
        RandomString(50),
        RandomString(34),
        Now() - (RandomInt() % 100000),
        static_cast<PlayerState>(RandomInt() % proto::common::player::PlayerState_ARRAYSIZE)
    };
}

class FakePlayerService : public rpc::api::player::PlayerService::Service
{
public:
    using PlayerMap = std::map<string, IVIPlayer>;
protected:
    static PlayerMap& SomePlayersNC()
    {
        struct Players
        {
            PlayerMap playerMap;
            Players()
            {
                const uint32_t count = RandomCount();
                for (uint32_t i = 0; i < count; ++i)
                {
                    IVIPlayer player(GeneratePlayer());
                    playerMap[player.playerId] = move(player);
                }
            }
        };
        static Players randomPlayers;
        return randomPlayers.playerMap;
    }
public:
    static const PlayerMap& SomePlayers()
    {
        return SomePlayersNC();
    }

    string lastTrackingId;

    proto::api::player::LinkPlayerRequest lastLinkPlayerRequest;
    ::grpc::Status LinkPlayer(::grpc::ServerContext* context, const ::ivi::proto::api::player::LinkPlayerRequest* request, ::ivi::proto::api::player::LinkPlayerAsyncResponse* response) override
    {
        lastLinkPlayerRequest = *request;
        lastTrackingId = RandomString(25);
        response->set_tracking_id(lastTrackingId);
        response->set_player_state(ECast(PlayerState::PENDING_LINKED));
        return ::grpc::Status::OK;
    }
    
    proto::api::player::GetPlayersRequest lastGetPlayersRequest;
    ::grpc::Status GetPlayers(::grpc::ServerContext* context, const ::ivi::proto::api::player::GetPlayersRequest* request, ::ivi::proto::api::player::IVIPlayers* response) override
    {
        lastGetPlayersRequest = *request;
        transform(SomePlayers().begin(), SomePlayers().end(), RepeatedFieldBackInserter(response->mutable_ivi_players()),
            [](const FakePlayerService::PlayerMap::value_type& player) { return player.second.ToProto(); });
        return ::grpc::Status::OK;
    }

    proto::api::player::GetPlayerRequest lastGetPlayerRequest;
    ::grpc::Status GetPlayer(::grpc::ServerContext* context, const ::ivi::proto::api::player::GetPlayerRequest* request, ::ivi::proto::api::player::IVIPlayer* response) override
    {
        lastGetPlayerRequest = *request;
        *response = SomePlayers().at(request->player_id()).ToProto();
        return ::grpc::Status::OK;
    }
};

using PlayerServiceTest = ClientTest<FakePlayerService>;

TEST_F(PlayerServiceTest, LinkPlayer)
{
    struct RPCTestData
    {
        string playerId{ RandomString(12) },
               email{ RandomString(13) },
               displayName{ RandomString(14) },
               requestIp{ RandomString(15) };
    };

    auto checkSuccessResult = [&](const RPCTestData& data, const IVIResultPlayerStateChange& result)
    {
        const proto::api::player::LinkPlayerRequest& request(m_service.lastLinkPlayerRequest);
        ASSERT_TRUE(result.Success());
        ASSERT_EQ(result.Payload().trackingId, m_service.lastTrackingId);
        ASSERT_EQ(result.Payload().playerState, PlayerState::PENDING_LINKED);

        ASSERT_EQ(request.player_id(), data.playerId);
        ASSERT_EQ(request.email(), data.email);
        ASSERT_EQ(request.display_name(), data.displayName);
        ASSERT_EQ(request.request_ip(), data.requestIp);
    };

    auto syncCaller = [&](const RPCTestData& data)
    {
        return m_syncManager->PlayerClient().LinkPlayer(data.playerId, data.email, data.displayName, data.requestIp);
    };

    auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultPlayerStateChange&)>& callback)
    {
        return m_asyncManager->PlayerClient().LinkPlayer(data.playerId, data.email, data.displayName, data.requestIp, callback);
    };

    ClientTest::template UnaryTest<RPCTestData>(checkSuccessResult, syncCaller, asyncCaller);
}

TEST_F(PlayerServiceTest, GetPlayer)
{
    for(int i = 0; i < FakePlayerService::SomePlayers().size(); ++i)
    {
        struct RPCTestData
        {
            string playerId{ RandomKey(FakePlayerService::SomePlayers()) };
        };

        auto checkSuccessResult = [&](const RPCTestData& data, const IVIResultPlayer& result)
        {
            const proto::api::player::GetPlayerRequest& request(m_service.lastGetPlayerRequest);
            ASSERT_TRUE(result.Success());
            ASSERT_EQ(request.player_id(), data.playerId);
            ASSERT_EQ(result.Payload().playerId, data.playerId);
            CheckEq(result.Payload(), FakePlayerService::SomePlayers().at(data.playerId));
        };

        auto syncCaller = [&](const RPCTestData& data)
        {
            return m_syncManager->PlayerClient().GetPlayer(data.playerId);
        };

        auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultPlayer&)>& callback)
        {
            m_asyncManager->PlayerClient().GetPlayer(data.playerId, callback);
        };

        ClientTest::template UnaryTest<RPCTestData>(checkSuccessResult, syncCaller, asyncCaller);
    }
}

TEST_F(PlayerServiceTest, GetPlayers)
{
    struct RPCTestData
    {
        time_t createdTimestamp = Now();
        int32_t pageSize = RandomInt();
        SortOrder sortOrder = static_cast<SortOrder>(RandomInt() % proto::common::sort::SortOrder_ARRAYSIZE);
    };

    auto checkSuccessResult = [&](const RPCTestData& data, const IVIResultPlayerList& result)
    {
        const proto::api::player::GetPlayersRequest& request(m_service.lastGetPlayersRequest);
        ASSERT_TRUE(result.Success());
        ASSERT_EQ(request.created_timestamp(), data.createdTimestamp);
        ASSERT_EQ(request.page_size(), data.pageSize);
        ASSERT_EQ(request.sort_order(), ECast(data.sortOrder));

        std::for_each(result.Payload().begin(), result.Payload().end(),
            [](const IVIPlayer& player)
            { CheckEq(player, FakePlayerService::SomePlayers().at(player.playerId)); });
    };

    auto syncCaller = [&](const RPCTestData& data)
    {
        return m_syncManager->PlayerClient().GetPlayers(data.createdTimestamp, data.pageSize, data.sortOrder);
    };

    auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultPlayerList&)>& callback)
    {
        return m_asyncManager->PlayerClient().GetPlayers(data.createdTimestamp, data.pageSize, data.sortOrder, callback);
    };

    ClientTest::template UnaryTest<RPCTestData>(checkSuccessResult, syncCaller, asyncCaller);
}

IVIOrderAddress GenerateOrderAddress()
{
    return
    {
        RandomString(8),
        RandomString(9),
        RandomString(10),
        RandomString(11),
        RandomString(12),
        RandomString(13),
        RandomString(14),
        RandomString(15),
        RandomString(16),
    };
}

IVIPurchasedItems GeneratePurchasedItems()
{
    return
    {
        RandomStringList(12, 16),
        RandomString(13),
        RandomString(15),
        RandomFloatString(1.f, 1000.f),
        RandomString(2),
        GenerateMetadata()
    };
}

IVIPurchasedItemsList GeneratePurchasedItemsList()
{
    IVIPurchasedItemsList retVal;
    const int count = RandomCount();
    for (int i = 0; i < count; ++i)
        retVal.push_back(GeneratePurchasedItems());
    return retVal;
}

IVIOrder GenerateOrder()
{
    return
    {
        RandomString(32),
        RandomString(5),
        RandomString(16),
        RandomFloatString(0.f, 100.f),
        RandomFloatString(100.f, 200.f),
        GenerateOrderAddress(),
        GenerateJsonString(),
        RandomString(12),
        RandomString(14),
        EnvironmentId,
        Now() - (RandomInt() % 100000),
        GenerateJsonString(),
        static_cast<PaymentProviderId>(RandomInt() % proto::api::order::payment::PaymentProviderId_ARRAYSIZE),
        static_cast<OrderState>(RandomInt() % proto::common::order::OrderState_ARRAYSIZE)
    };
}

void CheckEq(const IVIOrderAddress& lhs, const IVIOrderAddress& rhs)
{
    ASSERT_NE(&lhs, &rhs);
    ASSERT_EQ(lhs.firstName, rhs.firstName);
    ASSERT_EQ(lhs.lastName, rhs.lastName);
    ASSERT_EQ(lhs.addressLine1, rhs.addressLine1);
    ASSERT_EQ(lhs.addressLine2, rhs.addressLine2);
    ASSERT_EQ(lhs.city, rhs.city);
    ASSERT_EQ(lhs.state, rhs.state);
    ASSERT_EQ(lhs.postalCode, rhs.postalCode);
    ASSERT_EQ(lhs.countryName, rhs.countryName);
    ASSERT_EQ(lhs.countryIsoAlpha2, rhs.countryIsoAlpha2);
}

void CheckEq(const IVIOrder& lhs, const IVIOrder& rhs)
{
    ASSERT_NE(&lhs, &rhs);
    ASSERT_EQ(lhs.orderId, rhs.orderId);
    ASSERT_EQ(lhs.storeId, rhs.storeId);
    ASSERT_EQ(lhs.buyerPlayerId, rhs.buyerPlayerId);
    ASSERT_EQ(lhs.tax, rhs.tax);
    ASSERT_EQ(lhs.total, rhs.total);
    CheckEq(lhs.address, rhs.address);
    ASSERT_EQ(lhs.paymentProviderId, rhs.paymentProviderId);
    ASSERT_EQ(lhs.metadata, rhs.metadata);
    ASSERT_EQ(lhs.createdBy, rhs.createdBy);
    ASSERT_EQ(lhs.requestIp, rhs.requestIp);
    ASSERT_EQ(lhs.environmentId, rhs.environmentId);
    ASSERT_EQ(lhs.environmentId, EnvironmentId);
    ASSERT_EQ(lhs.orderStatus, rhs.orderStatus);
    ASSERT_EQ(lhs.createdTimestamp, rhs.createdTimestamp);
    ASSERT_EQ(lhs.bitpayInvoice, rhs.bitpayInvoice);
}

template<class LHSListType, class RHSListType>
void CheckEqList(const LHSListType& lhs, const RHSListType& rhs)
{
    ASSERT_NE(static_cast<const void*>(&lhs), static_cast<const void*>(&rhs));
    ASSERT_EQ(lhs.size(), rhs.size());
    auto lhsIt(lhs.begin());
    auto rhsIt(rhs.begin());

    while (lhsIt != lhs.end() || rhsIt != rhs.end())    // || in case we have a buggy list impl
    {
        ASSERT_EQ(*lhsIt, *rhsIt);
        ++lhsIt;
        ++rhsIt;
    }
}

class FakeOrderService : public rpc::api::order::OrderService::Service
{
public:
    using OrderMap = std::map<string, IVIOrder>;
protected:
    static OrderMap& SomeOrdersNC()
    {
        struct Orders
        {
            OrderMap orders;
            Orders()
            {
                const int count = RandomCount();
                for (int i = 0; i < count; ++i)
                {
                    IVIOrder order(GenerateOrder());
                    orders[order.orderId] = move(order);
                }
            }
        };
        static Orders randomOrders;
        return randomOrders.orders;
    }
public:
    static const OrderMap& SomeOrders()
    {
        return SomeOrdersNC();
    }

    proto::api::order::CreateOrderRequest lastCreateOrderRequest;
    ::grpc::Status CreateOrder(::grpc::ServerContext* context, const proto::api::order::CreateOrderRequest* request, ::ivi::proto::api::order::Order* response) override
    {
        lastCreateOrderRequest = *request;

        IVIOrder newOrder
        {
            RandomString(10),
            request->store_id(),
            request->buyer_player_id(),
            RandomFloatString(),
            request->sub_total(),
            IVIOrderAddress::FromProto(request->address()),
            request->has_metadata() ? GoogleStructToJsonString(request->metadata()) : "",
            RandomString(14),
            request->request_ip(),
            EnvironmentId,
            Now(),
            GenerateJsonString(),
            ECast(request->payment_provider_id()),
            OrderState::STARTED
        };

        SomeOrdersNC()[newOrder.orderId] = newOrder;
        *response = newOrder.ToProto();

        return ::grpc::Status::OK;
    }

    proto::api::order::GetOrderRequest lastGetOrderRequest;
    ::grpc::Status GetOrder(::grpc::ServerContext* context, const proto::api::order::GetOrderRequest* request, ::ivi::proto::api::order::Order* response) override
    {
        lastGetOrderRequest = *request;
        auto it(SomeOrders().find(request->order_id()));
        *response = it->second.ToProto();
        return ::grpc::Status::OK;
    }

    proto::api::order::FinalizeOrderRequest lastFinalizeOrderRequest;
    proto::api::order::FinalizeOrderAsyncResponse lastFinalizeOrderAsyncResponse;
    ::grpc::Status FinalizeOrder(::grpc::ServerContext* context, const proto::api::order::FinalizeOrderRequest* request, ::ivi::proto::api::order::FinalizeOrderAsyncResponse* response) override
    {
        lastFinalizeOrderRequest = *request;
        
        auto it(SomeOrdersNC().find(request->order_id()));
        IVIOrder& order(it->second);
        order.orderStatus = OrderState::PROCESSING;

        response->set_order_status(ECast(order.orderStatus));
        response->set_success(true);
        response->set_processor_response(RandomString(22));
        response->set_payment_instrument_type(RandomString(15));

        if (request->fraud_session_id().size() > 0)
        {
            response->mutable_fraud_score()->set_fraud_score(RandomInt());
            response->mutable_fraud_score()->set_fraud_omniscore(RandomFloatString());
        }

        lastFinalizeOrderAsyncResponse = *response;
        return ::grpc::Status::OK;
    }
};

using OrderClientTest = ClientTest<FakeOrderService>;

TEST_F(OrderClientTest, CreatePrimaryOrder)
{
    const uint32_t count = RandomCount();
    for(uint32_t i = 0; i < count; ++i)
    {
        struct RPCTestData
        {
            string storeId{ RandomString(12) };
            string buyerPlayerId{ RandomString(14) };
            BigDecimal subTotal{ RandomFloatString(1.f, 100.f) };
            IVIOrderAddress address{ GenerateOrderAddress() };
            PaymentProviderId paymentProviderId = static_cast<PaymentProviderId>(RandomInt() % proto::api::order::payment::PaymentProviderId_ARRAYSIZE);
            IVIPurchasedItemsList purchasedItems{ GeneratePurchasedItemsList() };
            string metadata{ GenerateJsonString() };
            string requestIp{ RandomString(11) };
        };

        auto checkSuccessResult = [&](const RPCTestData& data, const IVIResultOrder& result)
        {
            const proto::api::order::CreateOrderRequest& request(m_service.lastCreateOrderRequest);
            ASSERT_TRUE(result.Success());
            auto it(FakeOrderService::SomeOrders().find(result.Payload().orderId));
            ASSERT_NE(it, FakeOrderService::SomeOrders().end());
            CheckEq(result.Payload(), it->second);

            ASSERT_EQ(data.storeId, result.Payload().storeId);
            ASSERT_EQ(data.buyerPlayerId, result.Payload().buyerPlayerId);
            ASSERT_EQ(data.subTotal, request.sub_total());
            CheckEq(data.address, it->second.address);
            CheckEq(data.address, result.Payload().address);
            ASSERT_EQ(data.paymentProviderId, result.Payload().paymentProviderId);
            ASSERT_EQ(data.metadata, result.Payload().metadata);
            ASSERT_EQ(data.requestIp, result.Payload().requestIp);

            ASSERT_EQ(data.purchasedItems.size(), request.purchased_items().purchased_items().size());
            auto itDataPI(data.purchasedItems.begin());
            auto itReqPI(request.purchased_items().purchased_items().begin());
            while (itDataPI != data.purchasedItems.end() || itReqPI != request.purchased_items().purchased_items().end()) // || in case some bug gets introduced with the list types, should cause a crash
            {
                const IVIPurchasedItems& lhs(*itDataPI);
                const proto::api::order::ItemTypeOrder& rhs(*itReqPI);
                CheckEqList(lhs.gameInventoryIds, rhs.game_inventory_ids());
                ASSERT_EQ(lhs.itemName, rhs.item_name());
                ASSERT_EQ(lhs.gameItemTypeId, rhs.game_item_type_id());
                ASSERT_EQ(lhs.amountPaid, rhs.amount_paid());
                ASSERT_EQ(lhs.currency, rhs.currency());
                CheckEq(lhs.metadata, IVIMetadata::FromProto(rhs.metadata()));
                ++itDataPI;
                ++itReqPI;
            }
        };

        auto syncCaller = [&](const RPCTestData& data)
        {
            return m_syncManager->OrderClient().CreatePrimaryOrder(
                data.storeId, data.buyerPlayerId, data.subTotal, data.address, data.paymentProviderId, data.purchasedItems, data.metadata, data.requestIp);
        };

        auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultOrder&)> callback)
        {
            m_asyncManager->OrderClient().CreatePrimaryOrder(
                data.storeId, data.buyerPlayerId, data.subTotal, data.address, data.paymentProviderId, data.purchasedItems, data.metadata, data.requestIp, callback);
        };

        ClientTest::template UnaryTest<RPCTestData>(checkSuccessResult, syncCaller, asyncCaller);
    }
}

TEST_F(OrderClientTest, GetOrder)
{
    for (int i = 0; i < FakeOrderService::SomeOrders().size(); ++i)
    {
        struct RPCTestData
        {
            string orderId{ RandomKey(FakeOrderService::SomeOrders()) };
        };

        auto checkSuccessResult = [&](const RPCTestData& data, const IVIResultOrder& result)
        {
            const proto::api::order::GetOrderRequest& request(m_service.lastGetOrderRequest);
            ASSERT_TRUE(result.Success());
            ASSERT_EQ(data.orderId, request.order_id());
            auto it(FakeOrderService::SomeOrders().find(data.orderId));
            ASSERT_NE(it, FakeOrderService::SomeOrders().end());
            CheckEq(result.Payload(), it->second);
        };

        auto syncCaller = [&](const RPCTestData& data)
        {
            return m_syncManager->OrderClient().GetOrder(data.orderId);
        };
        
        auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultOrder&)>& callback)
        {
            m_asyncManager->OrderClient().GetOrder(data.orderId, callback);
        };

        ClientTest::template UnaryTest<RPCTestData>(checkSuccessResult, syncCaller, asyncCaller);
    }
}

TEST_F(OrderClientTest, FinalizeOrder)
{
    for (int i = 0; i < FakeOrderService::SomeOrders().size(); ++i)
    {
        struct RPCTestData
        {
            string orderId{ RandomKey(FakeOrderService::SomeOrders()) };
            bool isBraintree{ RandomBool() };
            string tokenOrInvoiceId{ RandomString(21) };
            string braintreeNonce{ RandomString(19) };
            string fraudSessionId{ RandomBool() ? RandomString(10) : "" };
        };

        auto checkSuccessResult = [&](const RPCTestData& data, const IVIResultFinalizeOrderResponse& result)
        {
            const proto::api::order::FinalizeOrderRequest& request(m_service.lastFinalizeOrderRequest);
            const proto::api::order::FinalizeOrderAsyncResponse& response(m_service.lastFinalizeOrderAsyncResponse);
            ASSERT_TRUE(result.Success());
            
            ASSERT_EQ(request.environment_id(), EnvironmentId);
            ASSERT_EQ(request.order_id(), data.orderId);
            ASSERT_EQ(request.fraud_session_id(), data.fraudSessionId);
            if (data.isBraintree)
            {
                ASSERT_TRUE(request.payment_request_data().has_braintree());
                ASSERT_EQ(request.payment_request_data().braintree().braintree_client_token(), data.tokenOrInvoiceId);
                ASSERT_EQ(request.payment_request_data().braintree().braintree_payment_nonce(), data.braintreeNonce);
            }
            else
            {
                ASSERT_TRUE(request.payment_request_data().has_bitpay());
                ASSERT_EQ(request.payment_request_data().bitpay().invoice_id(), data.tokenOrInvoiceId);
            }

            ASSERT_EQ(response.order_status(), ECast(result.Payload().orderStatus));
            ASSERT_EQ(response.success(), result.Payload().success);
            ASSERT_EQ(response.payment_instrument_type(), result.Payload().paymentInstrumentType);
            ASSERT_EQ(response.transaction_id(), result.Payload().transactionId);
            ASSERT_EQ(response.processor_response(), result.Payload().processorResponse);

            if (data.fraudSessionId.size() > 0)
            {
                ASSERT_TRUE(response.has_fraud_score());
                ASSERT_TRUE(result.Payload().scoreIsValid);
                ASSERT_EQ(response.fraud_score().fraud_score(), result.Payload().fraudScore);
                ASSERT_EQ(response.fraud_score().fraud_omniscore(), result.Payload().omniScore);
            }
            else
            {
                ASSERT_FALSE(response.has_fraud_score());
                ASSERT_FALSE(result.Payload().scoreIsValid);
            }

            auto it(FakeOrderService::SomeOrders().find(data.orderId));
            ASSERT_NE(it, FakeOrderService::SomeOrders().end());
            const IVIOrder& order(it->second);
            ASSERT_EQ(order.orderStatus, OrderState::PROCESSING);
            ASSERT_EQ(result.Payload().orderStatus, OrderState::PROCESSING);
        };

        auto syncCaller = [&](const RPCTestData& data)
        {
            if (data.isBraintree)
                return m_syncManager->OrderClient().FinalizeBraintreeOrder(data.orderId, data.tokenOrInvoiceId, data.braintreeNonce, data.fraudSessionId);
            else
                return m_syncManager->OrderClient().FinalizeBitpayOrder(data.orderId, data.tokenOrInvoiceId, data.fraudSessionId);
        };

        auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultFinalizeOrderResponse)>& callback)
        {
            if (data.isBraintree)
                return m_asyncManager->OrderClient().FinalizeBraintreeOrder(data.orderId, data.tokenOrInvoiceId, data.braintreeNonce, data.fraudSessionId, callback);
            else
                return m_asyncManager->OrderClient().FinalizeBitpayOrder(data.orderId, data.tokenOrInvoiceId, data.fraudSessionId, callback);
        };

        ClientTest::template UnaryTest<RPCTestData>(checkSuccessResult, syncCaller, asyncCaller);
    }
}

class FakePaymentService : public rpc::api::payment::PaymentService::Service
{
public:
    
    string lastBraintreeToken;
    proto::api::payment::CreateTokenRequest lastCreateTokenRequest;
    ::grpc::Status GenerateClientToken(::grpc::ServerContext* context, const ::ivi::proto::api::payment::CreateTokenRequest* request, ::ivi::proto::api::payment::Token* response) override
    {
        lastCreateTokenRequest = *request;
        lastBraintreeToken = RandomString(256);
        response->mutable_braintree()->set_token(lastBraintreeToken);
        return ::grpc::Status::OK;
    }
};

using PaymentServiceTest = ClientTest<FakePaymentService>;

TEST_F(PaymentServiceTest, GetToken)
{
    struct RPCTestData
    {
        string playerId{ RandomString(40) };
    };

    auto checkSuccessResult = [&](const RPCTestData& data, const IVIResultToken& result)
    {
        const proto::api::payment::CreateTokenRequest& request(m_service.lastCreateTokenRequest);
        ASSERT_TRUE(result.Success());
        ASSERT_EQ(data.playerId, request.braintree().player_id());
        ASSERT_EQ(result.Payload().braintreeToken, m_service.lastBraintreeToken);
        ASSERT_EQ(result.Payload().paymentProviderId, PaymentProviderId::BRAINTREE);
    };

    auto syncCaller = [&](const RPCTestData& data)
    {
        return m_syncManager->PaymentClient().GetToken(PaymentProviderId::BRAINTREE, data.playerId);
    };

    auto asyncCaller = [&](const RPCTestData& data, const function<void(const IVIResultToken&)>& callback)
    {
        return m_asyncManager->PaymentClient().GetToken(PaymentProviderId::BRAINTREE, data.playerId, callback);
    };

    ClientTest::template UnaryTest<RPCTestData>(checkSuccessResult, syncCaller, asyncCaller);
}

// Wrap the boilerplate code for the stream tests
template<
    typename TService,
    typename TUpdateProto,
    typename TConfirmProto,
    typename TUpdatePopulator>
class FakeStreamT : public TService
{

public:
    using UpdateMap = std::map<string, TUpdateProto>;

public:

    static const UpdateMap& SomeUpdates()
    {
        struct Updates
        {
            UpdateMap updateMap;
            Updates()
            {
                const int count = RandomCount();
                for (int i = 0; i < count; ++i)
                {
                    TUpdatePopulator()(updateMap);
                }
            }
        };
        static Updates randomUpdates;
        return randomUpdates.updateMap;
    }

    rpc::streams::Subscribe lastSubscribe;
    std::atomic_int subscribeCount{0};

    list<TConfirmProto> receivedConfirmRequests;
    std::atomic_int confirmCount{0};

protected:
    
    ::grpc::Status PushUpdatesAndWait(const rpc::streams::Subscribe* request, ::grpc::ServerWriter<TUpdateProto> * writer)
    {
        ++subscribeCount;
        lastSubscribe = *request;

        ::grpc::WriteOptions options;
        options.set_write_through();

        std::for_each(SomeUpdates().begin(), SomeUpdates().end(),
            [&](const typename UpdateMap::value_type& protoPair)
            { writer->Write(protoPair.second, options); });

        SpinWait([&]() { return subscribeCount == 1; });
        return ::grpc::Status::OK;
    }

    ::grpc::Status OnConfirmationReceived(const TConfirmProto* request)
    {
        ++confirmCount;
        receivedConfirmRequests.push_back(*request);
        return ::grpc::Status::OK;
    }
};

template<class TService, IVIStreamCallbacks* TCallbacks>
class ClientStreamTest : public ClientTest<TService, TCallbacks>
{
public:
    template<typename TConfirmChecker>
    void StreamTest(const int32_t& UpdateCounter, TConfirmChecker&& confirmChecker)
    {
        SpinWait([&]() { return this->m_service.subscribeCount == 0; });

        while (UpdateCounter < TService::SomeUpdates().size())
        {
            ASSERT_TRUE(this->m_asyncManager->Poll());
        }

        ASSERT_EQ(this->m_service.subscribeCount, 1);
        ASSERT_EQ(this->m_service.lastSubscribe.environment_id(), EnvironmentId);

        SpinWait([&]() { return this->m_service.confirmCount < UpdateCounter; });

        ASSERT_EQ(UpdateCounter, TService::SomeUpdates().size());
        ASSERT_EQ(this->m_service.confirmCount, TService::SomeUpdates().size());

        std::for_each(this->m_service.receivedConfirmRequests.begin(), this->m_service.receivedConfirmRequests.end(), confirmChecker);

        ASSERT_EQ(this->m_service.subscribeCount, 1);
        this->m_service.subscribeCount = 0;
    }
};

struct ISUPopulator
{
    static rpc::streams::item::ItemStatusUpdate GenerateISU()
    {
        rpc::streams::item::ItemStatusUpdate retVal;
        retVal.set_game_inventory_id(RandomString(12));
        retVal.set_game_item_type_id(RandomString(24));
        retVal.set_player_id(RandomString(10));
        retVal.set_metadata_uri(RandomString(66));
        retVal.set_tracking_id(RandomString(34));
        retVal.set_dgoods_id(RandomInt<int64_t>());
        retVal.set_serial_number(RandomInt());
        retVal.set_item_state(static_cast<proto::common::item::ItemState>(RandomInt() % proto::common::item::ItemState_ARRAYSIZE));
        return retVal;
    }

    void operator()(std::map<string, rpc::streams::item::ItemStatusUpdate>& isuMap)
    {
        auto isu(GenerateISU());
        isuMap[isu.game_inventory_id()] = move(isu);
    }
};

class FakeItemStream : 
    public FakeStreamT<
        rpc::streams::item::ItemStream::Service, 
        rpc::streams::item::ItemStatusUpdate,
        rpc::streams::item::ItemStatusConfirmRequest,
        ISUPopulator>
{
    ::grpc::Status ItemStatusStream(::grpc::ServerContext* context, const rpc::streams::Subscribe* request, ::grpc::ServerWriter< rpc::streams::item::ItemStatusUpdate>* writer) override
    {
        return PushUpdatesAndWait(request, writer);
    }

    ::grpc::Status ItemStatusConfirmation(::grpc::ServerContext* context, const rpc::streams::item::ItemStatusConfirmRequest* request, ::google::protobuf::Empty* response) override
    {
        return OnConfirmationReceived(request);
    }
};

static int32_t FakeOnItemUpdatedCount = 0;
void FakeOnItemUpdated(const IVIItemStatusUpdate& update)
{
    auto it(FakeItemStream::SomeUpdates().find(update.gameInventoryId));
    ASSERT_NE(it, FakeItemStream::SomeUpdates().end());
    const auto& proto(it->second);
    ASSERT_EQ(update.gameInventoryId, proto.game_inventory_id());
    ASSERT_EQ(update.gameItemTypeId, proto.game_item_type_id());
    ASSERT_EQ(update.playerId, proto.player_id());
    ASSERT_EQ(update.metadataUri, proto.metadata_uri());
    ASSERT_EQ(update.trackingId, proto.tracking_id());
    ASSERT_EQ(update.dgoodsId, proto.dgoods_id());
    ASSERT_EQ(update.serialNumber, proto.serial_number());
    ASSERT_EQ(update.itemState, ECast(proto.item_state()));
    ++FakeOnItemUpdatedCount;
}

static IVIStreamCallbacks FakeItemStreamCallbacks{ &FakeOnItemUpdated };
using ItemStreamTest = ClientStreamTest<FakeItemStream, &FakeItemStreamCallbacks>;

TEST_F(ItemStreamTest, UpdateStream)
{
    auto confirmChecker = [](const rpc::streams::item::ItemStatusConfirmRequest& request)
    {
        auto it(FakeItemStream::SomeUpdates().find(request.game_inventory_id()));
        ASSERT_NE(it, FakeItemStream::SomeUpdates().end());
        const auto& update(it->second);
        ASSERT_EQ(update.tracking_id(), request.tracking_id());
        ASSERT_EQ(update.item_state(), request.item_state());
        ASSERT_EQ(EnvironmentId, request.environment_id());
    };

    ClientStreamTest::template StreamTest(FakeOnItemUpdatedCount, confirmChecker);
}

struct ITSUPopulator
{
    static rpc::streams::itemtype::ItemTypeStatusUpdate GenerateITSU()
    {
        rpc::streams::itemtype::ItemTypeStatusUpdate retVal;
        retVal.set_game_item_type_id(RandomString(22));
        retVal.set_base_uri(RandomString(55));
        retVal.set_tracking_id(RandomString(33));
        retVal.set_current_supply(RandomInt());
        retVal.set_issued_supply(RandomInt());
        retVal.set_issue_time_span(RandomInt());
        retVal.set_item_type_state(static_cast<proto::common::itemtype::ItemTypeState>(RandomInt() % proto::common::itemtype::ItemTypeState_ARRAYSIZE));
        return retVal;
    }

    void operator()(std::map<string, rpc::streams::itemtype::ItemTypeStatusUpdate>& isuMap)
    {
        auto isu(GenerateITSU());
        isuMap[isu.game_item_type_id()] = move(isu);
    }
};

class FakeItemTypeStream : public
    FakeStreamT<
        rpc::streams::itemtype::ItemTypeStatusStream::Service,
        rpc::streams::itemtype::ItemTypeStatusUpdate,
        rpc::streams::itemtype::ItemTypeStatusConfirmRequest,
        ITSUPopulator >
{
public:
    ::grpc::Status ItemTypeStatusStream(::grpc::ServerContext* context, const ::ivi::rpc::streams::Subscribe* request, ::grpc::ServerWriter< ::ivi::rpc::streams::itemtype::ItemTypeStatusUpdate>* writer) override
    {
        return PushUpdatesAndWait(request, writer);
    }

    ::grpc::Status ItemTypeStatusConfirmation(::grpc::ServerContext* context, const ::ivi::rpc::streams::itemtype::ItemTypeStatusConfirmRequest* request, ::google::protobuf::Empty* response) override
    {
        return OnConfirmationReceived(request);
    }
};

static int32_t FakeOnItemTypeUpdatedCount = 0;
void FakeOnItemTypeUpdated(const IVIItemTypeStatusUpdate& update)
{
    auto it(FakeItemTypeStream::SomeUpdates().find(update.gameItemTypeId));
    ASSERT_NE(it, FakeItemTypeStream::SomeUpdates().end());
    const auto& proto(it->second);
    ASSERT_EQ(update.gameItemTypeId, proto.game_item_type_id());
    ASSERT_EQ(update.baseUri, proto.base_uri());
    ASSERT_EQ(update.trackingId, proto.tracking_id());
    ASSERT_EQ(update.currentSupply, proto.current_supply());
    ASSERT_EQ(update.issueTimeSpan, proto.issue_time_span());
    ASSERT_EQ(update.itemTypeState, ECast(proto.item_type_state()));
    ++FakeOnItemTypeUpdatedCount;
}

static IVIStreamCallbacks FakeItemTypeStreamCallbacks{ OnItemUpdated(), &FakeOnItemTypeUpdated };
using ItemTypeStreamTest = ClientStreamTest<FakeItemTypeStream, &FakeItemTypeStreamCallbacks>;

TEST_F(ItemTypeStreamTest, UpdateStream)
{
    auto confirmChecker = [](const rpc::streams::itemtype::ItemTypeStatusConfirmRequest& request)
    {
        auto it(FakeItemTypeStream::SomeUpdates().find(request.game_item_type_id()));
        ASSERT_NE(it, FakeItemTypeStream::SomeUpdates().end());
        const auto& update(it->second);
        ASSERT_EQ(update.tracking_id(), request.tracking_id());
        ASSERT_EQ(update.item_type_state(), request.item_type_state());
        ASSERT_EQ(EnvironmentId, request.environment_id());
    };

    ClientStreamTest::template StreamTest(FakeOnItemTypeUpdatedCount, confirmChecker);
}

struct OSUPopulator
{
    rpc::streams::order::OrderStatusUpdate GenerateOSU()
    {
        rpc::streams::order::OrderStatusUpdate retVal;
        retVal.set_order_id(RandomString(32));
        retVal.set_order_state(static_cast<proto::common::order::OrderState>(RandomInt() % proto::common::order::OrderState_ARRAYSIZE));
        return retVal;
    }

    void operator()(std::map<string, rpc::streams::order::OrderStatusUpdate>& osuMap)
    {
        auto osu(GenerateOSU());
        osuMap[osu.order_id()] = move(osu);
    }
};

class FakeOrderStream 
    : public FakeStreamT<
            rpc::streams::order::OrderStream::Service,
            rpc::streams::order::OrderStatusUpdate,
            rpc::streams::order::OrderStatusConfirmRequest,
            OSUPopulator>
{
    ::grpc::Status OrderStatusStream(::grpc::ServerContext* context, const ::ivi::rpc::streams::Subscribe* request, ::grpc::ServerWriter< ::ivi::rpc::streams::order::OrderStatusUpdate>* writer) override
    {
        return PushUpdatesAndWait(request, writer);
    }
    
    ::grpc::Status OrderStatusConfirmation(::grpc::ServerContext* context, const ::ivi::rpc::streams::order::OrderStatusConfirmRequest* request, ::google::protobuf::Empty* response) override
    {
        return OnConfirmationReceived(request);
    }
};

static int32_t FakeOnOrderUpdatedCount = 0;
void FakeOnOrderUpdated(const IVIOrderStatusUpdate& update)
{
    auto it(FakeOrderStream::SomeUpdates().find(update.orderId));
    ASSERT_NE(it, FakeOrderStream::SomeUpdates().end());
    const auto& proto(it->second);
    ASSERT_EQ(update.orderId, proto.order_id());
    ASSERT_EQ(update.orderState, ECast(proto.order_state()));
    ++FakeOnOrderUpdatedCount;
}

static IVIStreamCallbacks FakeOrderStreamCallbacks = { OnItemUpdated(), OnItemTypeUpdated(), &FakeOnOrderUpdated };
using OrderStreamTest = ClientStreamTest<FakeOrderStream, &FakeOrderStreamCallbacks>;

TEST_F(OrderStreamTest, UpdateStream)
{
    auto confirmChecker = [](const rpc::streams::order::OrderStatusConfirmRequest& request)
    {
        auto it(FakeOrderStream::SomeUpdates().find(request.order_id()));
        ASSERT_NE(it, FakeOrderStream::SomeUpdates().end());
        const auto& update(it->second);
        ASSERT_EQ(update.order_id(), request.order_id());
        ASSERT_EQ(update.order_state(), request.order_state());
        ASSERT_EQ(EnvironmentId, request.environment_id());
    };

    ClientStreamTest::template StreamTest(FakeOnOrderUpdatedCount, confirmChecker);
}

struct PSUPopulator
{
    rpc::streams::player::PlayerStatusUpdate GeneratePSU()
    {
        rpc::streams::player::PlayerStatusUpdate retVal;
        retVal.set_player_id(RandomString(32));
        retVal.set_tracking_id(RandomString(44));
        retVal.set_player_state(static_cast<proto::common::player::PlayerState>(RandomInt() % proto::common::player::PlayerState_ARRAYSIZE));
        return retVal;
    }

    void operator()(std::map<string, rpc::streams::player::PlayerStatusUpdate>& psuMap)
    {
        auto psu(GeneratePSU());
        psuMap[psu.player_id()] = move(psu);
    }
};

class FakePlayerStream :
    public FakeStreamT<
        rpc::streams::player::PlayerStream::Service,
        rpc::streams::player::PlayerStatusUpdate,
        rpc::streams::player::PlayerStatusConfirmRequest,
        PSUPopulator>
{
    ::grpc::Status PlayerStatusStream(::grpc::ServerContext* context, const ::ivi::rpc::streams::Subscribe* request, ::grpc::ServerWriter< ::ivi::rpc::streams::player::PlayerStatusUpdate>* writer) override
    {
        return PushUpdatesAndWait(request, writer);
    }
    ::grpc::Status PlayerStatusConfirmation(::grpc::ServerContext* context, const ::ivi::rpc::streams::player::PlayerStatusConfirmRequest* request, ::google::protobuf::Empty* response) override
    {
        return OnConfirmationReceived(request);
    }
};

static int32_t FakeOnPlayerUpdatedCount = 0;
void FakeOnPlayerUpdated(const IVIPlayerStatusUpdate& update)
{
    auto it(FakePlayerStream::SomeUpdates().find(update.playerId));
    ASSERT_NE(it, FakePlayerStream::SomeUpdates().end());
    const auto& proto(it->second);
    ASSERT_EQ(update.playerId, proto.player_id());
    ASSERT_EQ(update.playerState, ECast(proto.player_state()));
    ASSERT_EQ(update.trackingId, proto.tracking_id());
    ++FakeOnPlayerUpdatedCount;
}

static IVIStreamCallbacks FakePlayerStreamCallbacks = { OnItemUpdated(), OnItemTypeUpdated(), OnOrderUpdated(), &FakeOnPlayerUpdated };
using PlayerStreamTest = ClientStreamTest<FakePlayerStream, &FakePlayerStreamCallbacks>;

TEST_F(PlayerStreamTest, UpdateStream)
{
    auto confirmChecker = [](const rpc::streams::player::PlayerStatusConfirmRequest& request)
    {
        auto it(FakePlayerStream::SomeUpdates().find(request.player_id()));
        ASSERT_NE(it, FakePlayerStream::SomeUpdates().end());
        const auto& update(it->second);
        ASSERT_EQ(request.player_id(), update.player_id());
        ASSERT_EQ(request.player_state(), update.player_state());
        ASSERT_EQ(request.tracking_id(), update.tracking_id());
        ASSERT_EQ(EnvironmentId, request.environment_id());
    };

    ClientStreamTest::template StreamTest(FakeOnPlayerUpdatedCount, confirmChecker);
}


class FakeBrokenPaymentService : public rpc::api::payment::PaymentService::Service
{
public:
    bool ErrorMode = true;
    ::grpc::Status GenerateClientToken(::grpc::ServerContext* context, const ::ivi::proto::api::payment::CreateTokenRequest* request, ::ivi::proto::api::payment::Token* response) override
    {
        if (ErrorMode)
            return AnError(::grpc::StatusCode::NOT_FOUND);
        else
            return ::grpc::Status::OK;
    }
};

using UnaryErrorClientTest = ClientTest<FakeBrokenPaymentService>;

TEST_F(UnaryErrorClientTest, LogAndRecover)
{
    struct RPCTestData {};

    // Test gRPC error code - should give an RPC_FAIL-level log message
    {
        LogFilter::GetLogCounter().clear();
        int callbackCount = 0;
        auto checkResultFailure = [&callbackCount](const RPCTestData& data, const IVIResultToken& result)
        { 
            ASSERT_FALSE(result.Success()); 
            ASSERT_EQ(result.Status(), IVIResultStatus::NOT_FOUND);
            ++callbackCount;
        };
        auto syncPaymentCaller = [&](const RPCTestData& data)
        { return m_syncManager->PaymentClient().GetToken(PaymentProviderId::BRAINTREE, "abc123"); };
        auto asyncPaymentCaller = [&](const RPCTestData& data, const function<void(const IVIResultToken&)>& callback)
        { return m_asyncManager->PaymentClient().GetToken(PaymentProviderId::BRAINTREE, "abc123", callback); };

        m_service.ErrorMode = true;

        ASSERT_EQ(LogFilter::GetLogCounter()[static_cast<int>(LogLevel::RPC_FAIL)], 0);
        ClientTest::template UnaryTest<RPCTestData>(checkResultFailure, syncPaymentCaller, asyncPaymentCaller);
        ASSERT_EQ(LogFilter::GetLogCounter()[static_cast<int>(LogLevel::RPC_FAIL)], IVI_LOGGING_LEVEL >= 3 ? 2 : 0);
        ClientTest::template UnaryTest<RPCTestData>(checkResultFailure, syncPaymentCaller, asyncPaymentCaller);
        ASSERT_EQ(LogFilter::GetLogCounter()[static_cast<int>(LogLevel::RPC_FAIL)], IVI_LOGGING_LEVEL >= 3 ? 4 : 0);
        ASSERT_EQ(LogFilter::GetLogCounter()[static_cast<int>(LogLevel::WARNING)], 0);
        ASSERT_EQ(LogFilter::GetLogCounter()[static_cast<int>(LogLevel::CRITICAL)], 0);
        ASSERT_EQ(callbackCount, 4);
    }

    // Test another gRPC error code - should give an RPC_FAIL-level log message
    {
        LogFilter::GetLogCounter().clear();
        int callbackCount = 0;
        auto checkResultFailure = [&callbackCount](const RPCTestData& data, const IVIResultOrder& result)
        { 
            ASSERT_FALSE(result.Success()); 
            ASSERT_EQ(result.Status(), IVIResultStatus::UNIMPLEMENTED);
            ++callbackCount;
        };

        auto syncOrderCaller = [&](const RPCTestData& data)
        { return m_syncManager->OrderClient().GetOrder("abc123"); };
        auto asyncOrderCaller = [&](const RPCTestData& data, const function<void(const IVIResultOrder&)>& callback)
        { return m_asyncManager->OrderClient().GetOrder("abc123", callback); };

        ASSERT_EQ(LogFilter::GetLogCounter()[static_cast<int>(LogLevel::WARNING)], 0);
        ClientTest::template UnaryTest<RPCTestData>(checkResultFailure, syncOrderCaller, asyncOrderCaller);
        ASSERT_EQ(LogFilter::GetLogCounter()[static_cast<int>(LogLevel::RPC_FAIL)], IVI_LOGGING_LEVEL >= 3 ? 2 : 0);
        ClientTest::template UnaryTest<RPCTestData>(checkResultFailure, syncOrderCaller, asyncOrderCaller);
        ASSERT_EQ(LogFilter::GetLogCounter()[static_cast<int>(LogLevel::RPC_FAIL)], IVI_LOGGING_LEVEL >= 3 ? 4 : 0);
        ASSERT_EQ(LogFilter::GetLogCounter()[static_cast<int>(LogLevel::WARNING)], 0);
        ASSERT_EQ(LogFilter::GetLogCounter()[static_cast<int>(LogLevel::CRITICAL)], 0);
        ASSERT_EQ(callbackCount, 4);
    }
}


static IVIStreamCallbacks StreamErrorTestingCallbacks =
{
    [](const IVIItemStatusUpdate&) { ASSERT_TRUE(false); },
    [](const IVIItemTypeStatusUpdate&) { ASSERT_TRUE(false); },
    [](const IVIOrderStatusUpdate&) { ASSERT_TRUE(false); },
    [](const IVIPlayerStatusUpdate&) { ASSERT_TRUE(false); }
};

using StreamErrorClientTest = ClientStreamTest<rpc::streams::player::PlayerStream::Service, &StreamErrorTestingCallbacks>;

TEST_F(StreamErrorClientTest, LogAndRecover)
{
    const auto startTime(std::chrono::system_clock::now());
    const auto testTimeLimit(std::chrono::seconds(30));

    bool shouldContinue = true;

    SpinWait([&]()
        {
            EXPECT_TRUE(shouldContinue);    // sanity check our own loop logic
            shouldContinue = m_asyncManager->Poll();
            EXPECT_TRUE(shouldContinue);
            return shouldContinue && std::chrono::system_clock::now() - startTime < testTimeLimit;
        });

    if (IVI_LOGGING_LEVEL >= 2)
    {
        ASSERT_GT(LogFilter::GetLogCounter()[static_cast<int>(LogLevel::WARNING)], 0);
    }
    ASSERT_TRUE(shouldContinue);
}

