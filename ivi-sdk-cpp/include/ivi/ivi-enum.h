#ifndef __IVI_ENUMS_H__
#define __IVI_ENUMS_H__

#define IVI_DEFINE_ECAST(X, Y) \
    constexpr X ECast(Y val) { return static_cast<X>(val); } \
    constexpr Y ECast(X val) { return static_cast<Y>(val); }

/*
* Contains copies of proto-defined enums to prevent header dependency pollution.
* Important - any switch() statements should have a default case as it is 
* possible for the server to send a newly-defined value to an old 
* client; the SDK does not check for this.
* Protobuf provides IsValid() functions to detect this as well,
* extern'ed below for convenience and requiring linkage to the generated lib.
* This header defines type-safe ECast functions for conversion between the 
* versions defined in this header and the protobuf-defined versions.
* Value parity is guaranteed at compile time by ivi-enum.cpp.
*/
namespace ivi
{
    namespace proto
    {
        namespace api
        {
            namespace order
            {
                namespace payment
                {
                    enum PaymentProviderId : int;
                    extern bool PaymentProviderId_IsValid(int value);
                }
            }
        }
        namespace common
        {
            namespace finalization 
            { 
                enum Finalized : int; 
                extern bool Finalized_IsValid(int value);
            }
            namespace item 
            { 
                enum ItemState : int; 
                extern bool ItemState_IsValid(int value);
            }
            namespace itemtype 
            { 
                enum ItemTypeState : int; 
                extern bool ItemTypeState_IsValid(int value);
            }
            namespace order 
            { 
                enum OrderState : int; 
                extern bool OrderState_IsValid(int value);
            }
            namespace player 
            { 
                enum PlayerState : int; 
                extern bool PlayerState_IsValid(int value);
            }
            namespace sort 
            { 
                enum SortOrder : int; 
                extern bool SortOrder_IsValid(int value);
            }
        }
    }

    enum class Finalized : char
    {
        ALL = 0,
        YES = 1,
        NO = 2
    };

    IVI_DEFINE_ECAST(Finalized, proto::common::finalization::Finalized);

    enum class ItemState : char
    {
        PENDING_ISSUED = 0,
        ISSUED = 1,
        PENDING_LISTED = 2,
        LISTED = 3,
        PENDING_TRANSFERRED = 4,
        TRANSFERRED = 5,
        PENDING_SALE = 6,
        PENDING_BURNED = 7,
        BURNED = 8,
        FAILED = 9,
        PENDING_CLOSE_LISTING = 10,
        LISTING_CLOSED = 11,
        UPDATED_METADATA = 12
    };

    IVI_DEFINE_ECAST(ItemState, proto::common::item::ItemState);

    enum class ItemTypeState : char 
    {
        PENDING_CREATE = 0,
        CREATED = 1,
        PENDING_FREEZE = 2,
        FROZEN = 3,
        SOLD_OUT = 4,
        EXPIRED = 5,
        FAILED = 6,
        UPDATED_METADATA = 7
    };

    IVI_DEFINE_ECAST(ItemTypeState, proto::common::itemtype::ItemTypeState);

    enum class OrderState : char 
    {
        STARTED = 0,
        PROCESSING = 1,
        ASSIGNING = 2,
        COMPLETE = 3,
        DECLINED = 4,
        FAILED = 5,
        PAID = 6,
        EXPIRED = 7
    };

    IVI_DEFINE_ECAST(OrderState, proto::common::order::OrderState);

    enum class PaymentProviderId : char 
    {
        BRAINTREE = 0,
        BITPAY = 1,
    };

    IVI_DEFINE_ECAST(PaymentProviderId, proto::api::order::payment::PaymentProviderId);

    enum class PlayerState : char 
    {
        PENDING_LINKED = 0,
        LINKED = 1,
        FAILED = 2,
    };

    IVI_DEFINE_ECAST(PlayerState, proto::common::player::PlayerState);

    enum class SortOrder : char
    {
        ASC = 0,
        DESC = 1,
    };

    IVI_DEFINE_ECAST(SortOrder, proto::common::sort::SortOrder);
}

#undef IVI_DEFINE_ECAST

#endif // __IVI_ENUMS_H__