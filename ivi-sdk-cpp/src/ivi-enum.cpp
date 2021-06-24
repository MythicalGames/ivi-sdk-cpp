#include "ivi/ivi-enum.h"
#include "ivi/ivi-util.h"

#include "ivi/generated/api/order/payment.pb.h"
#include "ivi/generated/common/finalization.pb.h"
#include "ivi/generated/common/item/definition.pb.h"
#include "ivi/generated/common/itemtype/definition.pb.h"
#include "ivi/generated/common/player/definition.pb.h"
#include "ivi/generated/common/order/definition.pb.h"
#include "ivi/generated/common/sort.pb.h"

// Sanity check all enum values at compile time

#define IVI_ENUM_CHECK(x, y) IVI_STATIC_ASSERT(static_cast<int>(x) == static_cast<int>(y))

using namespace ivi;
using namespace proto::common;

IVI_ENUM_CHECK(Finalized::ALL, finalization::ALL);
IVI_ENUM_CHECK(Finalized::YES, finalization::YES);
IVI_ENUM_CHECK(Finalized::NO, finalization::NO);
IVI_STATIC_ASSERT(finalization::Finalized_ARRAYSIZE == finalization::NO + 1);

IVI_ENUM_CHECK(ItemState::PENDING_ISSUED, item::PENDING_ISSUED);
IVI_ENUM_CHECK(ItemState::ISSUED, item::ISSUED);
IVI_ENUM_CHECK(ItemState::PENDING_LISTED, item::PENDING_LISTED);
IVI_ENUM_CHECK(ItemState::LISTED, item::LISTED);
IVI_ENUM_CHECK(ItemState::PENDING_TRANSFERRED, item::PENDING_TRANSFERRED);
IVI_ENUM_CHECK(ItemState::TRANSFERRED, item::TRANSFERRED);
IVI_ENUM_CHECK(ItemState::PENDING_SALE, item::PENDING_SALE);
IVI_ENUM_CHECK(ItemState::PENDING_BURNED, item::PENDING_BURNED);
IVI_ENUM_CHECK(ItemState::BURNED, item::BURNED);
IVI_ENUM_CHECK(ItemState::FAILED, item::FAILED);
IVI_ENUM_CHECK(ItemState::PENDING_CLOSE_LISTING, item::PENDING_CLOSE_LISTING);
IVI_ENUM_CHECK(ItemState::LISTING_CLOSED, item::LISTING_CLOSED);
IVI_ENUM_CHECK(ItemState::UPDATED_METADATA, item::UPDATED_METADATA);
IVI_STATIC_ASSERT(item::ItemState_ARRAYSIZE == item::UPDATED_METADATA + 1);

IVI_ENUM_CHECK(ItemTypeState::PENDING_CREATE, itemtype::PENDING_CREATE);
IVI_ENUM_CHECK(ItemTypeState::CREATED, itemtype::CREATED);
IVI_ENUM_CHECK(ItemTypeState::PENDING_FREEZE, itemtype::PENDING_FREEZE);
IVI_ENUM_CHECK(ItemTypeState::FROZEN, itemtype::FROZEN);
IVI_ENUM_CHECK(ItemTypeState::SOLD_OUT, itemtype::SOLD_OUT);
IVI_ENUM_CHECK(ItemTypeState::EXPIRED, itemtype::EXPIRED);
IVI_ENUM_CHECK(ItemTypeState::FAILED, itemtype::FAILED);
IVI_ENUM_CHECK(ItemTypeState::UPDATED_METADATA, itemtype::UPDATED_METADATA);
IVI_STATIC_ASSERT(itemtype::ItemTypeState_ARRAYSIZE == itemtype::UPDATED_METADATA + 1);

IVI_ENUM_CHECK(OrderState::STARTED, order::STARTED);
IVI_ENUM_CHECK(OrderState::PROCESSING, order::PROCESSING);
IVI_ENUM_CHECK(OrderState::ASSIGNING, order::ASSIGNING);
IVI_ENUM_CHECK(OrderState::COMPLETE, order::COMPLETE);
IVI_ENUM_CHECK(OrderState::DECLINED, order::DECLINED);
IVI_ENUM_CHECK(OrderState::FAILED, order::FAILED);
IVI_ENUM_CHECK(OrderState::PAID, order::PAID);
IVI_ENUM_CHECK(OrderState::EXPIRED, order::EXPIRED);
IVI_STATIC_ASSERT(order::OrderState_ARRAYSIZE == order::EXPIRED + 1);

IVI_ENUM_CHECK(PaymentProviderId::BRAINTREE, proto::api::order::payment::BRAINTREE);
IVI_ENUM_CHECK(PaymentProviderId::BITPAY, proto::api::order::payment::BITPAY);
IVI_STATIC_ASSERT(proto::api::order::payment::PaymentProviderId_ARRAYSIZE == proto::api::order::payment::BITPAY + 1);

IVI_ENUM_CHECK(PlayerState::PENDING_LINKED, player::PENDING_LINKED);
IVI_ENUM_CHECK(PlayerState::LINKED, player::LINKED);
IVI_ENUM_CHECK(PlayerState::FAILED, player::FAILED);
IVI_STATIC_ASSERT(player::PlayerState_ARRAYSIZE == player::FAILED + 1);

IVI_ENUM_CHECK(SortOrder::ASC, sort::ASC);
IVI_ENUM_CHECK(SortOrder::DESC, sort::DESC);
IVI_STATIC_ASSERT(sort::SortOrder_ARRAYSIZE == sort::DESC + 1);

#undef IVI_ENUM_CHECK
