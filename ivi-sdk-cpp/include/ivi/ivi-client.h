#ifndef __IVI_CLIENT_H__
#define __IVI_CLIENT_H__

#include "ivi/ivi-client-t.h"
#include "ivi/ivi-config.h"
#include "ivi/ivi-enum.h"
#include "ivi/ivi-executor.h"
#include "ivi/ivi-types.h"

/*
* The various service-level clients provided by IVI.
* IVIResult* types have three primary member functions:
*   Success()   - RPC succeeded
*   Status()    - The exact status code response of the RPC, useful for determing failures cause
                  and whether retry is appropriate.
*   Payload()   - optional, for calls that have response data (most calls do).
*                 Note - Payload is ONLY filled with valid response data if Success() == true,
*                 do not access otherwise.
*/

namespace ivi
{
    using IVIResult                     = IVIResultT<void>; // No Payload, just a Status

    using IVIResultItem                 = IVIResultT<IVIItem>;
    using IVIResultItemList             = IVIResultT<IVIItemList>;
    using IVIResultItemStateChange      = IVIResultT<IVIItemStateChange>;

    class IVI_SDK_API IVIItemClient
        : public IVIClientT<rpc::api::item::ItemService>
    {
    public:
        using                           IVIClientT<ServiceT>::IVIClientT;

         IVIResultItemStateChange       IssueItem(
                                            const string& gameInventoryId,
                                            const string& playerId,
                                            const string& itemName,
                                            const string& gameItemTypeId,
                                            const BigDecimal& amountPaid,
                                            const string& currency,
                                            const IVIMetadata& metadata,
                                            const string& storeId,
                                            const string& orderId,
                                            const string& requestIp);

         IVIResultItemStateChange       TransferItem(
                                            const string& gameInventoryId,
                                            const string& sourcePlayerId,
                                            const string& destPlayerId,
                                            const string& storeId);

         IVIResultItemStateChange       BurnItem(
                                            const string& gameInventoryId);


         IVIResultItem                  GetItem(
                                            const string& gameInventoryId,
                                            bool history = false);

         IVIResultItemList              GetItems(
                                            time_t createdTimestamp,
                                            int32_t pageSize,
                                            SortOrder sortOrder,
                                            Finalized finalized);

         IVIResult                      UpdateItemMetadata(
                                            const string& gameInventoryId,
                                            const IVIMetadata& metadata);

         IVIResult                      UpdateItemMetadata(
                                            const IVIMetadataUpdateList& updates);

    private:
        
        IVIResult                       UpdateItemMetadata(
                                            proto::api::item::UpdateItemMetadataRequest updateRequest);
    };

    class IVI_SDK_API IVIItemClientAsync
        : public IVIClientT<rpc::api::item::ItemService>
    {
    public:
        using                           IVIClientT<ServiceT>::IVIClientT;

        void                            IssueItem(
                                            const string& gameInventoryId,
                                            const string& playerId,
                                            const string& itemName,
                                            const string& gameItemTypeId,
                                            const BigDecimal& amountPaid,
                                            const string& currency,
                                            const IVIMetadata& metadata,
                                            const string& storeId,
                                            const string& orderId,
                                            const string& requestIp,
                                            const function<void(const IVIResultItemStateChange&)>& callback);

        void                            TransferItem(
                                            const string& gameInventoryId,
                                            const string& sourcePlayerId,
                                            const string& destPlayerId,
                                            const string& storeId,
                                            const function<void(const IVIResultItemStateChange&)>& callback);


        void                            BurnItem(
                                            const string& gameInventoryId,
                                            const function<void(const IVIResultItemStateChange&)>& callback);

        void                            GetItem(
                                            const string& gameInventoryId,
                                            const function<void(const IVIResultItem&)>& callback);

        void                            GetItem(
                                            const string& gameInventoryId,
                                            bool history,
                                            const function<void(const IVIResultItem&)>& callback);

        void                            GetItems(
                                            time_t createdTimestamp,
                                            int32_t pageSize,
                                            SortOrder sortOrder,
                                            Finalized finalized,
                                            const function<void(const IVIResultItemList&)>& callback);

        void                            UpdateItemMetadata(
                                            const string& gameInventoryId,
                                            const IVIMetadata& metadata,
                                            const function<void(const IVIResult&)>& callback);

        void                            UpdateItemMetadata(
                                            const IVIMetadataUpdateList& updates,
                                            const function<void(const IVIResult&)>& callback);

    private:

        void                            UpdateItemMetadata(
                                            proto::api::item::UpdateItemMetadataRequest updateRequest,
                                            const function<void(const IVIResult&)>& callback);
    };

    using IVIResultItemType             = IVIResultT<IVIItemType>;
    using IVIResultItemTypeList         = IVIResultT<IVIItemTypeList>;
    using IVIResultItemTypeStateChange  = IVIResultT<IVIItemTypeStateChange>;

    class IVI_SDK_API IVIItemTypeClient
        : public IVIClientT<rpc::api::itemtype::ItemTypeService>
    {
    public:
        using                           IVIClientT<ServiceT>::IVIClientT;

        IVIResultItemType               GetItemType(
                                            const string& gameItemTypeId);

        IVIResultItemTypeList           GetItemTypes(
                                            const StringList& gameItemTypeIds);

        IVIResultItemTypeStateChange    CreateItemType(
                                            const string& gameItemTypeId,
                                            const string& tokenName,
                                            const string& category,
                                            int32_t maxSupply,
                                            int32_t issueTimeSpan,
                                            bool burnable,
                                            bool transferable,
                                            bool sellable,
                                            const UUIDList& agreementIds,
                                            const IVIMetadata& metadata);
    
        IVIResultItemTypeStateChange    FreezeItemType(
                                            const string& gameItemTypeId);

        IVIResult                       UpdateItemTypeMetadata(
                                            const string& gameItemTypeId,
                                            const IVIMetadata& metadata);
    };

    class IVI_SDK_API IVIItemTypeClientAsync
        : public IVIClientT<rpc::api::itemtype::ItemTypeService>
    {
    public:
        using                           IVIClientT<ServiceT>::IVIClientT;

        void                            GetItemType(
                                            const string& gameItemTypeId,
                                            const function<void(const IVIResultItemType&)>& callback);

        void                            GetItemTypes(
                                            const StringList& gameItemTypeIds,
                                            const function<void(const IVIResultItemTypeList&)>& callback);

        void                            CreateItemType(
                                            const string& gameItemTypeId,
                                            const string& tokenName,
                                            const string& category,
                                            int32_t maxSupply,
                                            int32_t issueTimeSpan,
                                            bool burnable,
                                            bool transferable,
                                            bool sellable,
                                            const UUIDList& agreementIds,
                                            const IVIMetadata& metadata,
                                            const function<void(const IVIResultItemTypeStateChange&)>& callback);

        void                            FreezeItemType(
                                            const string& gameItemTypeId,
                                            const function<void(const IVIResultItemTypeStateChange&)>& callback);

        void                            UpdateItemTypeMetadata(
                                            const string& gameItemTypeId,
                                            const IVIMetadata& metadata,
                                            const function<void(const IVIResult&)>& callback);
    };

    using IVIResultPlayer               = IVIResultT<IVIPlayer>;
    using IVIResultPlayerList           = IVIResultT<IVIPlayerList>;
    using IVIResultPlayerStateChange    = IVIResultT<IVIPlayerStateChange>;

    class IVI_SDK_API IVIPlayerClient
        : public IVIClientT<rpc::api::player::PlayerService>
    {
    public:
        using                           IVIClientT<ServiceT>::IVIClientT;

        IVIResultPlayerStateChange      LinkPlayer(
                                            const string& playerId,
                                            const string& email,
                                            const string& displayName,
                                            const string& requestIp);

        IVIResultPlayer                 GetPlayer(
                                            const string& playerId);

        IVIResultPlayerList             GetPlayers(
                                            time_t createdTimestamp, 
                                            int32_t pageSize,
                                            SortOrder sortOrder);
    };

    class IVI_SDK_API IVIPlayerClientAsync
        : public IVIClientT<rpc::api::player::PlayerService>
    {
    public:
        using                           IVIClientT<ServiceT>::IVIClientT;

        void                            LinkPlayer(
                                            const string& playerId,
                                            const string& email,
                                            const string& displayName,
                                            const string& requestIp,
                                            const function<void(const IVIResultPlayerStateChange&)>& callback);

        void                            GetPlayer(
                                            const string& playerId,
                                            const function<void(const IVIResultPlayer&)>& callback);

        void                            GetPlayers(
                                            time_t createdTimestamp,
                                            int32_t pageSize,
                                            SortOrder sortOrder,
                                            const function<void(const IVIResultPlayerList&)>& callback);
    };

    using IVIResultOrder                    = IVIResultT<IVIOrder>;
    using IVIResultFinalizeOrderResponse    = IVIResultT<IVIFinalizeOrderResponse>;

    class IVI_SDK_API IVIOrderClient
        : public IVIClientT<rpc::api::order::OrderService>
    {
    public:
        using                           IVIClientT<ServiceT>::IVIClientT;

        IVIResultOrder                  GetOrder(
                                            const string& orderId);

        IVIResultOrder                  CreatePrimaryOrder(
                                            const string& storeId,
                                            const string& buyerPlayerId,
                                            const BigDecimal& subTotal,
                                            const IVIOrderAddress& address,
                                            PaymentProviderId paymentProviderId,
                                            const IVIPurchasedItemsList& purchasedItems,
                                            const string& metadata,
                                            const string& requestIp);

        IVIResultFinalizeOrderResponse  FinalizeBraintreeOrder(
                                            const string& orderId,
                                            const string& clientToken,
                                            const string& paymentNonce,
                                            const string& fraudSessionId);

        IVIResultFinalizeOrderResponse  FinalizeBitpayOrder(
                                            const string& orderId,
                                            const string& invoiceId,
                                            const string& fraudSessionId);

    private:
        
        IVIResultFinalizeOrderResponse  FinalizeOrder(
                                            const string& orderId,
                                            const string& fraudSessionId,
                                            proto::api::order::PaymentRequestProto paymentData);
    };

    class IVI_SDK_API IVIOrderClientAsync
        : public IVIClientT<rpc::api::order::OrderService>
    {
    public:
        using                           IVIClientT<ServiceT>::IVIClientT;

        void                            GetOrder(
                                            const string& orderId,
                                            const function<void(const IVIResultOrder&)> callback);

        void                            CreatePrimaryOrder(
                                            const string& storeId,
                                            const string& buyerPlayerId,
                                            const BigDecimal& subTotal,
                                            const IVIOrderAddress& address,
                                            PaymentProviderId paymentProviderId,
                                            const IVIPurchasedItemsList& purchasedItems,
                                            const string& metadata,
                                            const string& requestIp,
                                            const function<void(const IVIResultOrder&)> callback);

        void                            FinalizeBraintreeOrder(
                                            const string& orderId,
                                            const string& clientToken,
                                            const string& paymentNonce,
                                            const string& fraudSessionId,
                                            const function<void(const IVIResultFinalizeOrderResponse&)> callback);

        void                            FinalizeBitpayOrder(
                                            const string& orderId,
                                            const string& invoiceId,
                                            const string& fraudSessionId,
                                            const function<void(const IVIResultFinalizeOrderResponse&)> callback);

    private:
        
        void                            FinalizeOrder(
                                            const string& orderId,
                                            const string& fraudSessionId,
                                            proto::api::order::PaymentRequestProto paymentData,
                                            const function<void(const IVIResultFinalizeOrderResponse&)> callback);
    };

    using IVIResultToken                = IVIResultT<IVIToken>;

    class IVI_SDK_API IVIPaymentClient
        : public IVIClientT<rpc::api::payment::PaymentService>
    {
    public:
        using                           IVIClientT<ServiceT>::IVIClientT;

        IVIResultToken                  GetToken(
                                            PaymentProviderId id,
                                            const string& playerId);
    };

    class IVI_SDK_API IVIPaymentClientAsync
        : public IVIClientT<rpc::api::payment::PaymentService>
    {
    public:
        using                           IVIClientT<ServiceT>::IVIClientT;

        void                            GetToken(
                                            PaymentProviderId id,
                                            const string& playerId,
                                            const function<void(const IVIResultToken&)>& callback);
    };

    class IVIItemStreamClient;
    struct IVIItemStreamClientTraits
    {
        using StreamClient          = IVIItemStreamClient;
        using CallbackType          = OnItemUpdated;
        using MessageType           = rpc::streams::item::ItemStatusUpdate;
        using ParsedMessageType     = IVIItemStatusUpdate;
        using ServiceType           = rpc::streams::item::ItemStream;
    };

    class IVI_SDK_API IVIItemStreamClient final
        : public IVIStreamClientT<IVIItemStreamClientTraits>
    {
    public:
                                    IVIItemStreamClient(
                                        const IVIConfigurationPtr& configuration,
                                        const IVIConnectionPtr& conn,
                                        const OnItemUpdated& onItemUpdated);

        void                        Confirm(
                                        const string& gameInventoryId,
                                        const string& trackingId,
                                        ItemState itemState);
    };

    class IVIItemTypeStreamClient;
    struct IVIItemTypeStreamClientTraits
    {
        using StreamClient          = IVIItemTypeStreamClient;
        using CallbackType          = OnItemTypeUpdated;
        using MessageType           = rpc::streams::itemtype::ItemTypeStatusUpdate;
        using ParsedMessageType     = IVIItemTypeStatusUpdate;
        using ServiceType           = rpc::streams::itemtype::ItemTypeStatusStream;
    };

    class IVI_SDK_API IVIItemTypeStreamClient final
        : public IVIStreamClientT<IVIItemTypeStreamClientTraits>
    {
    public:
                                    IVIItemTypeStreamClient(
                                        const IVIConfigurationPtr& configuration,
                                        const IVIConnectionPtr& conn,
                                        const OnItemTypeUpdated& onItemTypeUpdated);

        void                        Confirm(
                                        const string& gameItemTypeId,
                                        const string& trackingId,
                                        ItemTypeState itemTypeState);
    };

    class IVIOrderStreamClient;
    struct IVIOrderStreamClientTraits
    {
        using StreamClient          = IVIOrderStreamClient;
        using CallbackType          = OnOrderUpdated;
        using MessageType           = rpc::streams::order::OrderStatusUpdate;
        using ParsedMessageType     = IVIOrderStatusUpdate;
        using ServiceType           = rpc::streams::order::OrderStream;
    };

    class IVI_SDK_API IVIOrderStreamClient final
        : public IVIStreamClientT<IVIOrderStreamClientTraits>
    {
    public:
                                    IVIOrderStreamClient(
                                        const IVIConfigurationPtr& configuration,
                                        const IVIConnectionPtr& conn,
                                        const OnOrderUpdated& onOrderUpdated);

        void                        Confirm(
                                        const string& orderId,
                                        OrderState orderState);
    };

    class IVIPlayerStreamClient;
    struct IVIPlayerStreamClientTraits
    {
        using StreamClient          = IVIPlayerStreamClient;
        using CallbackType          = OnPlayerUpdated;
        using MessageType           = rpc::streams::player::PlayerStatusUpdate;
        using ParsedMessageType     = IVIPlayerStatusUpdate;
        using ServiceType           = rpc::streams::player::PlayerStream;
    };

    class IVI_SDK_API IVIPlayerStreamClient final
        : public IVIStreamClientT<IVIPlayerStreamClientTraits>
    {
    public:
                                    IVIPlayerStreamClient(
                                        const IVIConfigurationPtr& configuration,
                                        const IVIConnectionPtr& conn,
                                        const OnPlayerUpdated& onOrderUpdated);

        void                        Confirm(
                                        const string& playerId,
                                        const string& trackingId,
                                        PlayerState playerState);
    };
}

#endif //__IVI_CLIENT_H__
