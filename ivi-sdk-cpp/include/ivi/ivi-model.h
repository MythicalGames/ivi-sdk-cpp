#ifndef __IVI_MODEL_H__
#define __IVI_MODEL_H__

#include "ivi/ivi-enum.h"
#include "ivi/ivi-sdk.h"
#include "ivi/ivi-types.h"

// The model structs below are purposely simple data types so
// API users need not worry about any non-trivial semantics
// and can rely on compiler-generated default functions and
// STL implementations, particularly copy/move semantics and NRVO.
namespace ivi
{
    string                              GoogleStructToJsonString(const google::protobuf::Struct& protoStruct);

    google::protobuf::Struct            JsonStringToGoogleStruct(const string& jsonString);

    struct IVI_SDK_API IVIMetadata
    {
        string                          name;
        string                          description;
        string                          image;
        string                          properties;

        static IVIMetadata              FromProto(const proto::common::Metadata& metadata);
        proto::common::Metadata         ToProto() const;
    };

    struct IVI_SDK_API IVIMetadataUpdate
    {
        string                                  gameInventoryId;
        IVIMetadata                             metadata;

        proto::api::item::UpdateItemMetadata    ToProto() const;
    };

    struct IVI_SDK_API IVIItem
    {
        string                          gameInventoryId;
        string                          gameItemTypeId;
        int64_t                         dgoodsId;
        string                          itemName;
        string                          playerId;
        string                          ownerSidechainAccount;
        int32_t                         serialNumber;
        string                          currencyBase;
        string                          metadataUri;
        string                          trackingId;
        IVIMetadata                     metadata;
        time_t                          createdTimestamp;
        time_t                          updatedTimestamp;
        ItemState                       itemState;

        static IVIItem                  FromProto(const proto::api::item::Item& item);
        proto::api::item::Item          ToProto() const;
    };

    struct IVI_SDK_API IVIItemType
    {
        string                          gameItemTypeId;
        int32_t                         maxSupply;
        int32_t                         currentSupply;
        int32_t                         issuedSupply;
        string                          issuer;
        int32_t                         issueTimeSpan;
        string                          category;
        string                          tokenName;
        string                          baseUri;
        UUIDList                        agreementIds;
        string                          trackingId;
        IVIMetadata                     metadata;
        time_t                          createdTimestamp;
        time_t                          updatedTimestamp;
        ItemTypeState                   itemTypeState;
        bool                            fungible : 1;
        bool                            burnable : 1;
        bool                            transferable : 1;
        bool                            finalized : 1;
        bool                            sellable : 1;

        static IVIItemType              FromProto(const proto::api::itemtype::ItemType& itemType);
        proto::api::itemtype::ItemType  ToProto() const;
    };

    struct IVI_SDK_API IVIOrderAddress
    {
        string                          firstName;
        string                          lastName;
        string                          addressLine1;
        string                          addressLine2;
        string                          city;
        string                          state;
        string                          postalCode;
        string                          countryName;
        string                          countryIsoAlpha2;

        static IVIOrderAddress          FromProto(const proto::api::order::Address& address);
        proto::api::order::Address      ToProto() const;
    };

    struct IVI_SDK_API IVIPurchasedItems
    {
        StringList                          gameInventoryIds;
        string                              itemName;
        string                              gameItemTypeId;
        BigDecimal                          amountPaid;
        string                              currency;
        IVIMetadata                         metadata;

        static IVIPurchasedItems            FromProto(const proto::api::order::ItemTypeOrder& purchasedItem);
        proto::api::order::ItemTypeOrder    ToProto() const;
    };

    struct IVI_SDK_API IVIOrder
    {
        string                              orderId;
        string                              storeId;
        string                              buyerPlayerId;
        BigDecimal                          tax;
        BigDecimal                          total;
        IVIOrderAddress                     address;
        string                              metadata;
        string                              createdBy;
        string                              requestIp;
        string                              environmentId;
        time_t                              createdTimestamp;
        string                              bitpayInvoice;
        PaymentProviderId                   paymentProviderId;
        OrderState                          orderStatus;

        static IVIOrder                     FromProto(const proto::api::order::Order& order);
        proto::api::order::Order            ToProto() const;
    };

    struct IVI_SDK_API IVIFinalizeOrderResponse
    {
        string                              paymentInstrumentType;
        string                              transactionId;
        string                              processorResponse;
        int32_t                             fraudScore; // check scoreIsValid first
        string                              omniScore;  // check scoreIsValid first
        OrderState                          orderStatus;
        bool                                success : 1;
        bool                                scoreIsValid : 1;

        static IVIFinalizeOrderResponse     FromProto(const proto::api::order::FinalizeOrderAsyncResponse& response);
    };

    struct IVI_SDK_API IVIItemStateChange
    {
        string                              gameInventoryId;
        string                              trackingId;
        ItemState                           itemState;
    };

    struct IVI_SDK_API IVIItemTypeStateChange
    {
        string                              gameItemTypeId;
        string                              trackingId;
        ItemTypeState                       itemTypeState;
    };

    struct IVI_SDK_API IVIPlayer
    {
        string                              playerId;
        string                              email;
        string                              displayName;
        string                              sidechainAccountName;
        string                              trackingId;
        time_t                              createdTimestamp;
        PlayerState                         playerState;

        static IVIPlayer                    FromProto(const proto::api::player::IVIPlayer& player);
        proto::api::player::IVIPlayer       ToProto() const;
    };

    struct IVI_SDK_API IVIToken
    {
        string                              braintreeToken;
        PaymentProviderId                   paymentProviderId;  // Only Braintree is supported currently

        static IVIToken                     FromProto(const proto::api::payment::Token& token);
    };

    struct IVI_SDK_API IVIPlayerStatusUpdate
    {
        string                              playerId;
        string                              trackingId;
        PlayerState                         playerState;

        static IVIPlayerStatusUpdate        FromProto(const rpc::streams::player::PlayerStatusUpdate& psu);
    };
	
    struct IVI_SDK_API IVIItemStatusUpdate
    {
        string                              gameInventoryId;
        string                              gameItemTypeId;
        string                              playerId;
        string                              metadataUri;
        string                              trackingId;
        int64_t                             dgoodsId;
        int32_t                             serialNumber;
        ItemState                           itemState;

        static IVIItemStatusUpdate          FromProto(const rpc::streams::item::ItemStatusUpdate& isu);
    };

    struct IVI_SDK_API IVIItemTypeStatusUpdate
    {
        string                              gameItemTypeId;
        string                              baseUri;
        string                              trackingId;
        int32_t                             currentSupply;
        int32_t                             issuedSupply;
        int32_t                             issueTimeSpan;
        ItemTypeState                       itemTypeState;

        static IVIItemTypeStatusUpdate      FromProto(const rpc::streams::itemtype::ItemTypeStatusUpdate& itsu);
    };

    struct IVI_SDK_API IVIOrderStatusUpdate
    {
        string                              orderId;
        OrderState                          orderState;

        static IVIOrderStatusUpdate         FromProto(const rpc::streams::order::OrderStatusUpdate& osu);
    };
} // namespace ivi

#endif // __IVI_MODEL_H__
