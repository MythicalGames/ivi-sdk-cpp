#ifndef __IVI_EXECUTOR_H__
#define __IVI_EXECUTOR_H__

#include "ivi/ivi-types.h"

namespace ivi
{
    /*
        void onItemUpdated(
                const ivi::string& gameInventoryId,
                const ivi::string& itemTypeId,
                const ivi::string& playerId,
                ivi::int64_t dGoodsId,
                ivi::int32_t serialNumber,
                const ivi::string& metadataUri,
                const ivi::string& trackingId,
                ivi::ItemState::ItemState itemState)
    */
    using OnItemUpdated 
            = function<void(
                const string&, 
                const string&, 
                const string&, 
                int64_t, 
                int32_t, 
                const string&, 
                const string&, 
                ItemState::ItemState)>;

    /*
      void onItemTypeUpdated(
                const ivi::string& gameItemTypeId,
                ivi::int32_t currentSupply,
                ivi::int32_t issuedSupply,
                const ivi::string& baseUri,
                ivi::int32_t issueTimespan,
                const ivi::string trackingId,
                ivi::ItemTypeState::ItemTypeState itemState)
    */
    using OnItemTypeUpdated
            = function<void(
                const string&,
                int32_t,
                int32_t,
                const string&,
                int32_t,
                const string,
                ItemTypeState::ItemTypeState)>;

    /*
      void onOrderUpdated(
                const ivi::string& orderId,
                ivi::OrderState::OrderState orderState)
    */
    using OnOrderUpdated
        = function<void(
                const string&,
                OrderState::OrderState)>;

    /*
      void onPlayerUpdated(
                const ivi::string& playerId,
                const ivi::string& trackingId,
                PlayerState::PlayerState playerState)
    */
    using OnPlayerUpdated
        = function<void(
                const string&,
                const string&,
                PlayerState::PlayerState)>;
}

#endif // __IVI_EXECUTOR_H__