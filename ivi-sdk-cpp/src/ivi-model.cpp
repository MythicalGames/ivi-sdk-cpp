
#include "ivi/ivi-model.h"
#include "ivi/ivi-util.h"


#include <limits>

#include "google/protobuf/util/json_util.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/stubs/status.h"

#include "ivi/generated/api/item/definition.pb.h"
#include "ivi/generated/api/itemtype/definition.pb.h"
#include "ivi/generated/api/order/definition.pb.h"
#include "ivi/generated/api/payment/definition.pb.h"
#include "ivi/generated/api/player/definition.pb.h"
#include "ivi/generated/common/common.pb.h"
#include "ivi/generated/common/item/definition.pb.h"
#include "ivi/generated/common/order/definition.pb.h"
#include "ivi/generated/streams/item/stream.pb.h"
#include "ivi/generated/streams/itemtype/stream.pb.h"
#include "ivi/generated/streams/order/stream.pb.h"
#include "ivi/generated/streams/player/stream.pb.h"

namespace ivi
{

string GoogleStructToJsonString(const google::protobuf::Struct& protoStruct)
{
    using namespace google::protobuf::util;
    string jsonString;    
    Status status(MessageToJsonString(protoStruct, &jsonString));
    IVI_CHECK(status.ok());
    return jsonString;
}

google::protobuf::Struct JsonStringToGoogleStruct(const string& jsonString)
{
    using namespace google::protobuf::util;
    google::protobuf::Struct protoStruct;
    if (!jsonString.empty())
    {
        Status status(JsonStringToMessage(jsonString, &protoStruct));
        IVI_CHECK(status.ok());
    }
    return protoStruct;
}

IVIMetadata IVIMetadata::FromProto(const proto::common::Metadata& metadata)
{
    return
    {
         metadata.name()
        ,metadata.description()
        ,metadata.image()
        ,metadata.has_properties() ? GoogleStructToJsonString(metadata.properties()) : ""
    };
}

proto::common::Metadata IVIMetadata::ToProto() const
{
    proto::common::Metadata retVal;
    retVal.set_name(name);
    retVal.set_description(description);
    retVal.set_image(image);
    if (properties.size() > 0)
    {
        *retVal.mutable_properties() = JsonStringToGoogleStruct(properties);
    }
    return retVal;
}

proto::api::item::UpdateItemMetadata IVIMetadataUpdate::ToProto() const
{
    proto::api::item::UpdateItemMetadata retVal;
    retVal.set_game_inventory_id(gameInventoryId);
    *retVal.mutable_metadata() = metadata.ToProto();
    return retVal;
}

IVIItem IVIItem::FromProto(const proto::api::item::Item& item)
{
    return
    {
         item.game_inventory_id()
        ,item.game_item_type_id()
        ,item.dgoods_id()
        ,item.item_name()
        ,item.player_id()
        ,item.owner_sidechain_account()
        ,item.serial_number()
        ,item.currency_base()
        ,item.metadata_uri()
        ,item.tracking_id()
        ,IVIMetadata::FromProto(item.metadata())
        ,item.created_timestamp()
        ,item.updated_timestamp()
        ,ECast(item.item_state())
    };
}

proto::api::item::Item IVIItem::ToProto() const
{
    proto::api::item::Item retVal;
    retVal.set_game_inventory_id(gameInventoryId);
    retVal.set_game_item_type_id(gameItemTypeId);
    retVal.set_dgoods_id(dgoodsId);
    retVal.set_item_name(itemName);
    retVal.set_player_id(playerId);
    retVal.set_owner_sidechain_account(ownerSidechainAccount);
    retVal.set_serial_number(serialNumber);
    retVal.set_currency_base(currencyBase);
    retVal.set_metadata_uri(metadataUri);
    retVal.set_tracking_id(trackingId);
    *retVal.mutable_metadata() = metadata.ToProto();
    retVal.set_created_timestamp(createdTimestamp);
    retVal.set_updated_timestamp(updatedTimestamp);
    retVal.set_item_state(ECast(itemState));
    return retVal;
}

IVIItemType IVIItemType::FromProto(const proto::api::itemtype::ItemType& itemType)
{
    return
    {
         itemType.game_item_type_id()
        ,itemType.max_supply()
        ,itemType.current_supply()
        ,itemType.issued_supply()
        ,itemType.issuer()
        ,itemType.issue_time_span()
        ,itemType.category()
        ,itemType.token_name()
        ,itemType.base_uri()
        ,{ itemType.agreement_ids().begin(), itemType.agreement_ids().end() }
        ,itemType.tracking_id()
        ,IVIMetadata::FromProto(itemType.metadata())
        ,itemType.created_timestamp()
        ,itemType.updated_timestamp()
        ,ECast(itemType.item_type_state())
        ,itemType.fungible()
        ,itemType.burnable()
        ,itemType.transferable()
        ,itemType.finalized()
        ,itemType.sellable()
    };
}

proto::api::itemtype::ItemType IVIItemType::ToProto() const
{
    proto::api::itemtype::ItemType itemType;
    itemType.set_game_item_type_id(gameItemTypeId);
    itemType.set_max_supply(maxSupply);
    itemType.set_current_supply(currentSupply);
    itemType.set_issued_supply(issuedSupply);
    itemType.set_issuer(issuer);
    itemType.set_issue_time_span(issueTimeSpan);
    itemType.set_category(category);
    itemType.set_token_name(tokenName);
    itemType.set_base_uri(baseUri);
    *itemType.mutable_agreement_ids() = { agreementIds.begin(), agreementIds.end() };
    itemType.set_tracking_id(trackingId);
    *itemType.mutable_metadata() = metadata.ToProto();
    itemType.set_created_timestamp(createdTimestamp);
    itemType.set_updated_timestamp(updatedTimestamp);
    itemType.set_item_type_state(ECast(itemTypeState));
    itemType.set_fungible(fungible);
    itemType.set_burnable(burnable);
    itemType.set_transferable(transferable);
    itemType.set_finalized(finalized);
    itemType.set_sellable(sellable);
    return itemType;
}

IVIOrderAddress IVIOrderAddress::FromProto(const proto::api::order::Address& address)
{
    return
    {
         address.first_name()
        ,address.last_name()
        ,address.address_line_1()
        ,address.address_line_2()
        ,address.city()
        ,address.state()
        ,address.postal_code()
        ,address.country_name()
        ,address.country_iso_alpha_2()
    };
}

proto::api::order::Address IVIOrderAddress::ToProto() const
{
    proto::api::order::Address retVal;
    retVal.set_first_name(firstName);
    retVal.set_last_name(lastName);
    retVal.set_address_line_1(addressLine1);
    retVal.set_address_line_2(addressLine2);
    retVal.set_city(city);
    retVal.set_state(state);
    retVal.set_postal_code(postalCode);
    retVal.set_country_name(countryName);
    retVal.set_country_iso_alpha_2(countryIsoAlpha2);
    return retVal;
}

IVIPurchasedItems IVIPurchasedItems::FromProto(const proto::api::order::ItemTypeOrder& purchasedItems)
{
    return
    {
        { purchasedItems.game_inventory_ids().begin(), purchasedItems.game_inventory_ids().end() }
        ,purchasedItems.item_name()
        ,purchasedItems.game_item_type_id()
        ,purchasedItems.amount_paid()
        ,purchasedItems.currency()
        ,IVIMetadata::FromProto(purchasedItems.metadata())
    };
}

proto::api::order::ItemTypeOrder IVIPurchasedItems::ToProto() const
{
    proto::api::order::ItemTypeOrder retVal;
    *retVal.mutable_game_inventory_ids() = { gameInventoryIds.begin(), gameInventoryIds.end() };
    retVal.set_item_name(itemName);
    retVal.set_game_item_type_id(gameItemTypeId);
    retVal.set_amount_paid(amountPaid);
    retVal.set_currency(currency);
    retVal.set_allocated_metadata(new proto::common::Metadata(metadata.ToProto()));
    return retVal;
}

IVIOrder IVIOrder::FromProto(const proto::api::order::Order& order)
{
    return
    {
         order.order_id()
        ,order.store_id()
        ,order.buyer_player_id()
        ,order.tax()
        ,order.total()
        ,IVIOrderAddress::FromProto(order.address())
        ,order.has_metadata() ? GoogleStructToJsonString(order.metadata()) : ""
        ,order.created_by()
        ,order.request_ip()
        ,order.environment_id()
        ,order.created_timestamp()
        ,order.has_payment_provider_data() && order.payment_provider_data().has_bitpay() ? 
               GoogleStructToJsonString(order.payment_provider_data().bitpay().invoice()) :
               ""
        ,ECast(order.payment_provider_id())
        ,ECast(order.order_status())
    };
}

proto::api::order::Order IVIOrder::ToProto() const
{
    proto::api::order::Order retVal;
    retVal.set_order_id(orderId);
    retVal.set_store_id(storeId);
    retVal.set_buyer_player_id(buyerPlayerId);
    retVal.set_tax(tax);
    retVal.set_total(total);
    *retVal.mutable_address() = address.ToProto();
    retVal.set_payment_provider_id(ECast(paymentProviderId));
    if (metadata.size() > 0)
        *retVal.mutable_metadata() = JsonStringToGoogleStruct(metadata);
    retVal.set_created_by(createdBy);
    retVal.set_request_ip(requestIp);
    retVal.set_environment_id(environmentId);
    retVal.set_order_status(ECast(orderStatus));
    retVal.set_created_timestamp(createdTimestamp);
    if (bitpayInvoice.size() > 0)
        *retVal.mutable_payment_provider_data()->mutable_bitpay()->mutable_invoice() = JsonStringToGoogleStruct(bitpayInvoice);
    return retVal;
}

IVIFinalizeOrderResponse IVIFinalizeOrderResponse::FromProto(const proto::api::order::FinalizeOrderAsyncResponse& response)
{
    return
    {
         response.payment_instrument_type()
        ,response.transaction_id()
        ,response.processor_response()
        ,response.has_fraud_score() ? response.fraud_score().fraud_score() : numeric_limits<int32_t>::max()
        ,response.has_fraud_score() ? response.fraud_score().fraud_omniscore() : string()
        ,ECast(response.order_status())
        ,response.success()
        ,response.has_fraud_score()
    };
}

IVIPlayer IVIPlayer::FromProto(const proto::api::player::IVIPlayer& player)
{
    return
    {
         player.player_id()
        ,player.email()
        ,player.display_name()
        ,player.sidechain_account_name()
        ,player.tracking_id()
        ,player.created_timestamp()
        ,ECast(player.player_state())
    };
}

proto::api::player::IVIPlayer IVIPlayer::ToProto() const
{
    proto::api::player::IVIPlayer retVal;
    retVal.set_player_id(playerId);
    retVal.set_email(email);
    retVal.set_display_name(displayName);
    retVal.set_sidechain_account_name(sidechainAccountName);
    retVal.set_tracking_id(trackingId);
    retVal.set_player_state(ECast(playerState));
    retVal.set_created_timestamp(createdTimestamp);
    return retVal;
}

IVIToken IVIToken::FromProto(const proto::api::payment::Token& token)
{
    if (!token.has_braintree())
    {
        IVI_LOG_CRITICAL("IVIToken UNSUPPORTED PAYMENT PROVIDER RECEIVED: ", token.provider_case());
        IVI_EXIT_FAILURE();
        return
        {
             "UNSUPPORTED"
            ,ECast(proto::api::order::payment::PaymentProviderId_INT_MAX_SENTINEL_DO_NOT_USE_)
        };
    }

    return
    {
         token.braintree().token()
        ,PaymentProviderId::BRAINTREE

    };
}

IVIPlayerStatusUpdate IVIPlayerStatusUpdate::FromProto(const rpc::streams::player::PlayerStatusUpdate& psu)
{
    return
    {
         psu.player_id()
        ,psu.tracking_id()
        ,ECast(psu.player_state())
    };
}

IVIItemStatusUpdate IVIItemStatusUpdate::FromProto(const rpc::streams::item::ItemStatusUpdate& isu)
{
    return
    {
         isu.game_inventory_id()
        ,isu.game_item_type_id()
        ,isu.player_id()
        ,isu.metadata_uri()
        ,isu.tracking_id()
        ,isu.dgoods_id()
        ,isu.serial_number()
        ,ECast(isu.item_state())
    };
}

IVIItemTypeStatusUpdate IVIItemTypeStatusUpdate::FromProto(const rpc::streams::itemtype::ItemTypeStatusUpdate& itsu)
{
    return
    {
         itsu.game_item_type_id()
        ,itsu.base_uri()
        ,itsu.tracking_id()
        ,itsu.current_supply()
        ,itsu.issued_supply()
        ,itsu.issue_time_span()
        ,ECast(itsu.item_type_state())
    };
}

IVIOrderStatusUpdate IVIOrderStatusUpdate::FromProto(const rpc::streams::order::OrderStatusUpdate& osu)
{
    return
    {
         osu.order_id()
        ,ECast(osu.order_state())
    };
}

} // namespace ivi
