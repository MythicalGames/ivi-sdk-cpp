
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <random>
#include <set>
#include <string>
#include <thread>
#include <tuple>

#include "ivi/ivi-client-mgr.h"
#include "ivi/ivi-config.h"
#include "ivi/ivi-model.h"
#include "ivi/ivi-util.h"    // for logging

using namespace ivi;


std::string MakeRandomString(std::int32_t len)
{
    static std::random_device randDev;
    static std::mt19937 randEng(randDev());

    const char alphanumerics[] =
        "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

    std::string retVal;

    retVal.reserve(len);
    for (int32_t i = 0; i < len; i += sizeof(std::uint32_t))
    {
        const std::uint32_t val(randEng());
        const std::uint8_t* valu8(reinterpret_cast<const std::uint8_t*>(&val));
        for (int32_t j = i; j < i + sizeof(val) && j < len; ++j)
        {
            retVal += alphanumerics[valu8[j % sizeof(val)] % (sizeof(alphanumerics)-1)];
        }
    }

    return retVal;
}

int main(int argc, char* argv[])
{
    IVI_LOG_INFO("Starting Example, sizeof(IVIClientManagerAsync)=", sizeof(IVIClientManagerAsync));

    // if you decide to go the dynamic linkage route, don't forget to check this
    IVI_CHECK(IVISDKAPIVersion() == IVI_SDK_API_VERSION);

    const char* iviHost = nullptr; 
    const char* iviEnv = nullptr;
    const char* iviApiKey = nullptr;
    if (argc >= 3)
    {
        IVI_LOG_INFO("Using connection info from command-line");
        iviEnv = argv[1];
        iviApiKey = argv[2];
        if (argc == 4)
            iviHost = argv[3];
    }
    else
    {
#if _MSC_VER
#pragma warning( disable : 4996 ) // warning C4996: 'getenv': This function or variable may be unsafe. Consider using _dupenv_s instead. To disable deprecation, use _CRT_SECURE_NO_WARNINGS. See online help for details.
#endif
        IVI_LOG_INFO("Trying to use connection info from env vars: IVI_ENV_ID + IVI_API_KEY + (optional) IVI_HOST");
        iviHost = std::getenv("IVI_HOST");
        iviEnv = std::getenv("IVI_ENV_ID");
        iviApiKey = std::getenv("IVI_API_KEY");
        IVI_CHECK(iviEnv != nullptr && iviApiKey != nullptr);
    }

    if (iviHost == nullptr)
        iviHost = "sdk-api.dev.iviengine.com:443";

    auto lastStreamCallback(std::chrono::system_clock::now());

    std::vector<std::string> playerIds;
    std::vector<std::string> itemTypeIds;
    std::vector<std::string> itemIds;

    // Set up all our Stream "executor" callbacks (see ivi-executors.h)
    auto itemUpdated = [&](
        const ivi::string& gameInventoryId,
        const ivi::string& itemTypeId,
        const ivi::string& playerId,
        ivi::int64_t dGoodsId,
        ivi::int32_t serialNumber,
        const ivi::string& metadataUri,
        const ivi::string& trackingId,
        ivi::proto::common::item::ItemState itemState)
    {
        IVI_LOG_INFO("OnItemUpdated: gameInventoryId=", gameInventoryId, " state=", itemState, " trackingId=", trackingId);
        lastStreamCallback = std::chrono::system_clock::now();
        itemIds.push_back(gameInventoryId);
    };

    auto itemTypeUpdated = [&](
        const ivi::string& gameItemTypeId,
        ivi::int32_t currentSupply,
        ivi::int32_t issuedSupply,
        const ivi::string& baseUri,
        ivi::int32_t issueTimespan,
        const ivi::string trackingId,
        ivi::ItemTypeState::ItemTypeState itemState)
    {
        IVI_LOG_INFO("OnItemTypeUpdated: gameItemTypeId=", gameItemTypeId, " itemState=", itemState, " trackingId=", trackingId);
        lastStreamCallback = std::chrono::system_clock::now();
        itemTypeIds.push_back(gameItemTypeId);
    };

    auto orderUpdated = [&lastStreamCallback](
        const string& orderId,
        OrderState::OrderState orderState)
    {
        IVI_LOG_INFO("OnOrderUpdated: orderId=", orderId, " orderState=", orderState);
        lastStreamCallback = std::chrono::system_clock::now();
    };

    auto playerUpdated = [&](
        const ivi::string& playerId,
        const ivi::string& trackingId,
        PlayerState::PlayerState playerState)
    {
        IVI_LOG_INFO("OnPlayerUpdated: playerId=", playerId, " trackingId=", trackingId, " playerState=", playerState);
        lastStreamCallback = std::chrono::system_clock::now();
        playerIds.push_back(playerId);
    };

    IVIConfigurationPtr configuration( std::make_shared<IVIConfiguration>(IVIConfiguration
    {
        iviEnv,
        iviApiKey,
        iviHost
    }));

    IVIConnectionPtr conn(IVIConnection::DefaultConnection(*configuration));

    IVIStreamCallbacks callbacks{
                            itemUpdated,
                            itemTypeUpdated,
                            orderUpdated,
                            playerUpdated };

    IVIClientManagerAsync clientMgr(
                            configuration,
                            conn,
                            callbacks);

    IVIClientManagerSync clientMgrSync(configuration, conn);

    IVI_LOG_INFO("Sending some sync calls with garbage data that should fail...");
    {
        IVIResultItemType itemTypeResult(clientMgrSync.ItemTypeClient().GetItemType("foobar"));
        IVI_CHECK(!itemTypeResult.Success());

        IVIResultItem itemResult(clientMgrSync.ItemClient().GetItem("foobar"));
        IVI_CHECK(!itemResult.Success());

        IVIResult updateItemResult(clientMgrSync.ItemClient().UpdateItemMetadata("foobar", { "foo", "bar", "png", "" }));
        IVI_CHECK(!itemResult.Success());

        IVIResultPlayer getPlayerResult(clientMgrSync.PlayerClient().GetPlayer("foobar"));
        IVI_CHECK(!itemResult.Success());
    }


    IVI_LOG_INFO("Parsing available outstanding stream messages...");
    for (int i = 0; i < 200; ++i)
    {
        clientMgr.Poll();
        using namespace std::chrono_literals;
        std::this_thread::sleep_for(10ms);
    }

    IVI_LOG_INFO("Creating some new players (async)...");
    for (int i = 0; i < 3; ++i)
    {
        clientMgr.PlayerClient().LinkPlayer(
            MakeRandomString(8),
            MakeRandomString(4) + "@iviengine.com",
            "Player " + std::to_string(i),
            "127.0.0.1",
            [&](const IVIResultPlayerUpdate& update)
            {
                IVI_CHECK(update.Success());
                if(update.Success())
                {
                    IVI_LOG_INFO("LinkPlayer: ", update.Payload().playerId);
                    playerIds.push_back(update.Payload().playerId);
                }
            });
    }

    IVI_LOG_INFO("Creating a new item type (async)...");
    const int maxSupply = 8;
    clientMgr.ItemTypeClient().CreateItemType(
        MakeRandomString(16),
        "TokenName " + MakeRandomString(2),
        "Category " + MakeRandomString(2),
        maxSupply,
        1,
        true,
        true,
        true,
        UUIDList(),
        IVIMetadata(),
        [&](const IVIResultItemTypeStateUpdate& update)
        {
            IVI_CHECK(update.Success());
            if (update.Success())
            {
                IVI_LOG_INFO("CreateItemType: " + update.Payload().gameItemTypeId);
            }
        });

    IVI_LOG_INFO("Waiting for new Players and Item Types to get streamed back to us...");
    using namespace std::chrono_literals;
    while (playerIds.size() < 2 || itemTypeIds.empty())
    {
        if (!clientMgr.Poll())
        {
            IVI_LOG_CRITICAL("Broken connection, quitting");
            return 1;
        }
        std::this_thread::sleep_for(10ms);
    }

    int numFailures = 0, numSuccesses = 0;
    const int totalAsync = 10, totalSync = 1;
    IVI_LOG_INFO("Synchronously creating 1 item; this can be slow - see log timestamps");
        for (int i = 0; i < totalSync; ++i)
    {
        IVIResultItemStateUpdate issueResult = clientMgrSync.ItemClient().IssueItem(
            MakeRandomString(8),
            playerIds[0],
            "First Item " + MakeRandomString(2),
            itemTypeIds[0],
            "1.00",
            "USD",
            IVIMetadata(),
            "ingame",
            "",
            "127.0.0.1");

        if (issueResult.Success())
        {
            ++numSuccesses;
            const auto& payload(issueResult.Payload());
            IVI_LOG_INFO("IssueItem: gameInventoryId=", payload.gameInventoryId, " state=", payload.itemState, " trackingId=", payload.trackingId);
        }
        else
        {
            ++numFailures;
        }
    }

    IVI_LOG_INFO("Issuing some items asynchronously...");

    for (int i = 0; i < totalAsync; ++i)
    {
        auto issueItemCallback = [&](const IVIResultItemStateUpdate& issueResult)
        {
            if (issueResult.Success())
            {
                const auto& payload(issueResult.Payload());
                IVI_LOG_INFO("IssueItem (async): gameInventoryId=", payload.gameInventoryId, " state=", payload.itemState, " trackingId=", payload.trackingId);
                ++numSuccesses;
            }
            else
            {
                ++numFailures;
            }
        };

        clientMgr.ItemClient().IssueItem(
            MakeRandomString(8),
            playerIds[i % playerIds.size()],
            "Item " + std::to_string(i) + " " + MakeRandomString(2),
            itemTypeIds[i % itemTypeIds.size()],
            "1.00",
            "USD",
            IVIMetadata(),
            "ingame",
            "",
            "127.0.0.1",
            issueItemCallback);
    }

    IVI_LOG_INFO("Waiting for issued items to be confirmed by the stream...");
    while (std::chrono::system_clock::now() - lastStreamCallback < 60s)
    {
        if (!clientMgr.Poll())
        {
            IVI_LOG_CRITICAL("Broken connection, quitting");
            return 2;
        }
        std::this_thread::sleep_for(10ms);
    }

    const auto numItems = itemIds.size();
    IVI_LOG_INFO("Burning ", numItems, " items");
    for (int i = 0; i < numItems; ++i)
    {
        std::string id(itemIds[i]);
        clientMgr.ItemClient().BurnItem(
            id,
            [id](const IVIResultItemStateUpdate& update)
            {
                if (update.Success())
                {
                    IVI_LOG_INFO("Burned: ", id);
                }
            });
    }

    lastStreamCallback = std::chrono::system_clock::now();

    IVI_LOG_INFO("Waiting for burned items and any other outstanding stream updates...");
    using namespace std::chrono_literals;
    while (std::chrono::system_clock::now() - lastStreamCallback < 120s)
    {
        if (!clientMgr.Poll())
        {
            IVI_LOG_CRITICAL("Broken connection, quitting");
            return 3;
        }
        std::this_thread::sleep_for(10ms);
    }

    IVI_LOG_INFO("IssuesItems maxSupply=", maxSupply, " total=", (totalAsync + totalSync), " numSuccesses=", numSuccesses, " numFailures=", numFailures);

    return 0;
}
