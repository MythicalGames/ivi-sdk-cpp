
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

IVIMetadata IVIMetadata::fromProto(const proto::common::Metadata& metadata)
{
    return
    {
         metadata.name()
        ,metadata.description()
        ,metadata.image()
        ,metadata.has_properties() ? GoogleStructToJsonString(metadata.properties()) : ""
    };
}

proto::common::Metadata IVIMetadata::toProto() const
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

proto::api::item::UpdateItemMetadata IVIMetadataUpdate::toProto() const
{
    proto::api::item::UpdateItemMetadata retVal;
    retVal.set_game_inventory_id(gameInventoryId);
    *retVal.mutable_metadata() = metadata.toProto();
    return retVal;
}

IVIItem IVIItem::fromProto(const proto::api::item::Item& item)
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
        ,IVIMetadata::fromProto(item.metadata())
        ,item.item_state()
        ,item.created_timestamp()
        ,item.updated_timestamp()
    };
}

IVIItemType IVIItemType::fromProto(const proto::api::itemtype::ItemType& itemType)
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
        ,IVIMetadata::fromProto(itemType.metadata())
        ,itemType.created_timestamp()
        ,itemType.updated_timestamp()
        ,itemType.item_type_state()
        ,itemType.fungible()
        ,itemType.burnable()
        ,itemType.transferable()
        ,itemType.finalized()
        ,itemType.sellable()
    };
}

IVIOrderAddress IVIOrderAddress::fromProto(const proto::api::order::Address& address)
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

proto::api::order::Address IVIOrderAddress::toProto() const
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

IVIPurchasedItems IVIPurchasedItems::fromProto(const proto::api::order::ItemTypeOrder& purchasedItems)
{
    return
    {
        { purchasedItems.game_inventory_ids().begin(), purchasedItems.game_inventory_ids().end() }
        ,purchasedItems.item_name()
        ,purchasedItems.game_item_type_id()
        ,purchasedItems.amount_paid()
        ,purchasedItems.currency()
        ,IVIMetadata::fromProto(purchasedItems.metadata())
    };
}

proto::api::order::ItemTypeOrder IVIPurchasedItems::toProto() const
{
    proto::api::order::ItemTypeOrder retVal;
    *retVal.mutable_game_inventory_ids() = { gameInventoryIds.begin(), gameInventoryIds.end() };
    retVal.set_item_name(itemName);
    retVal.set_game_item_type_id(gameItemTypeId);
    retVal.set_amount_paid(amountPaid);
    retVal.set_currency(currency);
    retVal.set_allocated_metadata(new proto::common::Metadata(metadata.toProto()));
    return retVal;
}

IVIOrder IVIOrder::fromProto(const proto::api::order::Order& order)
{
    return
    {
         order.order_id()
        ,order.store_id()
        ,order.buyer_player_id()
        ,order.tax()
        ,order.total()
        ,IVIOrderAddress::fromProto(order.address())
        ,order.listing_id()
        ,order.payment_provider_id()
        ,order.has_metadata() ? GoogleStructToJsonString(order.metadata()) : ""
        ,order.created_by()
        ,order.request_ip()
        ,order.environment_id()
        ,order.order_status()
        ,order.created_timestamp()
        ,order.has_payment_provider_data() && order.payment_provider_data().has_bitpay() ? 
               GoogleStructToJsonString(order.payment_provider_data().bitpay().invoice()) :
               ""
        ,order.line_items_case() == proto::api::order::Order::kPurchasedItems
        ,order.line_items_case() == proto::api::order::Order::kListingId
    };
}

IVIFinalizeOrderResponse IVIFinalizeOrderResponse::fromProto(const proto::api::order::FinalizeOrderAsyncResponse& response)
{
    return
    {
         response.order_status()
        ,response.payment_instrument_type()
        ,response.transaction_id()
        ,response.processor_response()
        ,response.has_fraud_score() ? response.fraud_score().fraud_score() : numeric_limits<int32_t>::max()
        ,response.has_fraud_score() ? response.fraud_score().fraud_omniscore() : string()
        ,response.success()
        ,response.has_fraud_score()
    };
}

IVIPlayer IVIPlayer::fromProto(const proto::api::player::IVIPlayer& player)
{
    return
    {
        player.player_id(),
        player.email(),
        player.display_name(),
        player.sidechain_account_name(),
        player.tracking_id(),
        player.player_state(),
        player.created_timestamp()
    };
}

IVIToken IVIToken::fromProto(const proto::api::payment::Token& token)
{
    if (!token.has_braintree())
    {
        IVI_LOG_CRITICAL("IVIToken UNSUPPORTED PAYMENT PROVIDER RECEIVED: ", token.provider_case());
        return
        {
            numeric_limits<PaymentProviderId::PaymentProviderId>::max(),
            "UNSUPPORTED"
        };
    }

    return
    {
        PaymentProviderId::BRAINTREE,
        token.braintree().token()
    };
}

} // namespace ivi
