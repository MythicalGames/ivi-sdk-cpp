#ifndef __IVI_TYPES_H__
#define __IVI_TYPES_H__

#include <algorithm>
#include <cstdint>
#include <ctime>
#include <list>
#include <memory>
#include <functional>
#include <string>
#include <tuple>
#include <utility>

/*
* Forward declarations and type aliases for shared types in the IVI SDK.
*/

namespace google
{
    namespace protobuf
    {
        class Empty;
        class Struct;
    }
}

namespace grpc
{
    class Channel;
    class ChannelArguments;
    class ClientContext;
    class CompletionQueue;
    template<typename R> class ClientAsyncResponseReader;
    class Status;
}

namespace ivi
{
    // STL aliases in case they need to be changed to alternate implementations
    using std::forward;
    using std::function;
    using std::get;
    using std::int32_t;
    using std::int64_t;
    using std::list;
    using std::make_shared;
    using std::move;
    using std::numeric_limits;
    using std::shared_ptr;
    using std::string;
    using std::time_t;
    using std::transform;
    using std::tuple;
    using std::unique_ptr;

    using UUID                      = string;
    using UUIDList                  = list<string>;


    using BigDecimal                = string;

    using StringList                = list<string>;

    struct IVIItem;
    using IVIItemList               = list<IVIItem>;

    struct IVIItemType;
    using IVIItemTypeList           = list<IVIItemType>;

    struct IVIItemStateUpdate;
    struct IVIItemTypeStateUpdate;

    struct IVIMetadata;

    struct IVIMetadataUpdate;
    using IVIMetadataUpdateList     = list<IVIMetadataUpdate>;

    struct IVIOrder;
    struct IVIOrderAddress;
    struct IVIFinalizeOrderResponse;

    struct IVIPlayer;
    using IVIPlayerList             = list<IVIPlayer>;

    struct IVIPlayerUpdate;

    struct IVIPurchasedItems;
    using IVIPurchasedItemsList      = list<IVIPurchasedItems>;

    struct IVIToken;

    struct IVIConnection;
    using IVIConnectionPtr          = shared_ptr<IVIConnection>;
    struct IVIConfiguration;
    using IVIConfigurationPtr       = shared_ptr<IVIConfiguration>;
    using ChannelPtr                = shared_ptr<grpc::Channel>;
    using CompletionQueuePtr        = shared_ptr<grpc::CompletionQueue>;

    class IVIItemClientAsync;
    class IVIItemTypeClientAsync;
    class IVIPaymentClientAsync;
    class IVIPlayerClientAsync;
    class IVIItemStreamClient;
    class IVIItemTypeStreamClient;
    class IVIOrderStreamClient;
    class IVIPlayerStreamClient;
    class IVIItemClient;
    class IVIItemTypeClient;
    class IVIPaymentClient;
    class IVIPlayerClient;

    enum class IVIResultStatus : int32_t
    {
        SUCCESS = 0,
        INVALID_ARGUMENT,
        NOT_FOUND,
        PERMISSION_DENIED,
        UNIMPLEMENTED,
        UNAUTHENTICATED,
        UNAVAILABLE,
        RESOURCE_EXHAUSTED,
        ABORTED,
        BAD_REQUEST,
        CONFLICT,
        SERVER_ERROR,
        NOT_AUTHORIZED,
        FORBIDDEN,
        TIMEOUT,
        UNPROCESSABLE_ENTITY,
        UNKNOWN_ERROR = numeric_limits<int32_t>::max()
    };

    namespace proto
    {
        namespace api
        {
            namespace item 
            { 
                class UpdateItemMetadata;
                class UpdateItemMetadataRequest;
                class Item;
            }
            namespace itemtype          { class ItemType; }
            namespace order
            {
                class Address;
                class FinalizeOrderAsyncResponse;
                class ItemTypeOrder;
                class Order;
                class PaymentRequestProto;
                namespace payment       { enum PaymentProviderId : int; }
            }
            namespace payment           { class Token; }
            namespace player            { class IVIPlayer; }
        }
        namespace common
        {
            class Metadata;
            namespace finalization      { enum Finalized : int; }
            namespace item              { enum ItemState : int; }
            namespace itemtype          { enum ItemTypeState : int; }
            namespace order             { enum OrderState : int; }
            namespace player            { enum PlayerState : int; }
            namespace sort              { enum SortOrder : int; }
        }
    }

    namespace rpc
    {
        namespace api
        {
            namespace item
            {
                class ItemService;
            }
            namespace itemtype
            {
                class ItemTypeService;
            }
            namespace order
            {
                class OrderService;
            }
            namespace payment
            {
                class PaymentService;
            }
            namespace player
            {
                class PlayerService;
            }
        }
        namespace streams
        {
            namespace item
            {
                class ItemStatusUpdate;
                class ItemStream;
            }
            namespace itemtype
            {
                class ItemTypeStatusUpdate;
                class ItemTypeStatusStream;
            }
            namespace order
            {
                class OrderStatusUpdate;
                class OrderStream;
            }
            namespace player
            {
                class PlayerStatusUpdate;
                class PlayerStream;
            }
        }
    }

    // Import protobuf C99 enums into namespace aliases 
    namespace Finalized             = proto::common::finalization;
    namespace ItemState             = proto::common::item;
    namespace ItemTypeState         = proto::common::itemtype;
    namespace OrderState            = proto::common::order;
    namespace PaymentProviderId     = proto::api::order::payment;
    namespace PlayerState           = proto::common::player;
    namespace SortOrder             = proto::common::sort;

    // Fat pointer encapsulation for gRPC tags.  See IVIClientManager comments for implementation details.
    using AsyncCallback = function<void(bool)>;

    template<class T>
    class NonCopyable
    {
    public:
        NonCopyable(const NonCopyable&) = delete;
        T& operator = (const T&) = delete;

    protected:
        NonCopyable() = default;
        ~NonCopyable() = default;
    };
}

#endif // __IVI_TYPES_H__