#ifndef __IVI_MODEL_H__
#define __IVI_MODEL_H__

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

        static IVIMetadata              fromProto(const proto::common::Metadata& metadata);
        proto::common::Metadata         toProto() const;
    };

    struct IVI_SDK_API IVIMetadataUpdate
    {
        string                                  gameInventoryId;
        IVIMetadata                             metadata;

        proto::api::item::UpdateItemMetadata    toProto() const;
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
        ItemState::ItemState            itemState;
        time_t                          createdTimestamp;
        time_t                          updatedTimestamp;

        static IVIItem                  fromProto(const proto::api::item::Item& item);
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
        ItemTypeState::ItemTypeState    itemTypeState;
        bool                            fungible;
        bool                            burnable;
        bool                            transferable;
        bool                            finalized;
        bool                            sellable;

        static IVIItemType              fromProto(const proto::api::itemtype::ItemType& itemType);
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

        static IVIOrderAddress          fromProto(const proto::api::order::Address& address);
        proto::api::order::Address      toProto() const;
    };

    struct IVI_SDK_API IVIPurchasedItems
    {
        StringList                          gameInventoryIds;
        string                              itemName;
        string                              gameItemTypeId;
        BigDecimal                          amountPaid;
        string                              currency;
        IVIMetadata                         metadata;

        static IVIPurchasedItems            fromProto(const proto::api::order::ItemTypeOrder& purchasedItem);
        
        proto::api::order::ItemTypeOrder    toProto() const;
    };

    struct IVI_SDK_API IVIOrder
    {
        string                                      orderId;
        string                                      storeId;
        string                                      buyerPlayerId;
        BigDecimal                                  tax;
        BigDecimal                                  total;
        IVIOrderAddress                             address;
        string                                      listingId;
        PaymentProviderId::PaymentProviderId        paymentProviderId;
        string                                      metadata;
        string                                      createdBy;
        string                                      requestIp;
        string                                      environmentId;
        OrderState::OrderState                      orderStatus;
        time_t                                      createdTimestamp;
        string                                      bitpayInvoice;
        bool                                        primarySale;
        bool                                        secondarySale;

        static IVIOrder                             fromProto(const proto::api::order::Order& order);
    };

    struct IVI_SDK_API IVIFinalizeOrderResponse
    {
        OrderState::OrderState              orderStatus;
        string                              paymentInstrumentType;
        string                              transactionId;
        string                              processorResponse;
        int32_t                             fraudScore;
        string                              omniScore;
        bool                                success;
        bool                                scoreIsValid;

        static IVIFinalizeOrderResponse     fromProto(const proto::api::order::FinalizeOrderAsyncResponse& response);
    };

    struct IVI_SDK_API IVIItemStateUpdate
    {
        string                              gameInventoryId;
        string                              trackingId;
        ItemState::ItemState                itemState;
    };

    struct IVI_SDK_API IVIItemTypeStateUpdate
    {
        string                              gameItemTypeId;
        string                              trackingId;
        ItemTypeState::ItemTypeState        itemTypeState;
    };

    struct IVI_SDK_API IVIPlayer
    {
        string                              playerIdd;
        string                              email;
        string                              displayName;
        string                              sidechainAccountName;
        string                              trackingId;
        PlayerState::PlayerState            playerState;
        time_t                              createdTimestamp;

        static IVIPlayer                    fromProto(const proto::api::player::IVIPlayer& player);
    };

    struct IVI_SDK_API IVIPlayerUpdate
    {
        string                              playerId;
        string                              trackingId;
        PlayerState::PlayerState            playerState;
    };

    struct IVI_SDK_API IVIToken
    {
        PaymentProviderId::PaymentProviderId    paymentProviderId;  // Only Braintree is supported currently
        string                                  braintreeToken;

        static IVIToken                         fromProto(const proto::api::payment::Token& token);
    };

} // namespace ivi

#endif // __IVI_MODEL_H__
