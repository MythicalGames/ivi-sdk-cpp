#ifndef __IVI_EXECUTOR_H__
#define __IVI_EXECUTOR_H__

#include "ivi/ivi-types.h"

namespace ivi
{
    /*
        void onItemUpdated(
                const IVIItemStatusUpdate& update)
    */
    using OnItemUpdated 
            = function<void(
                const IVIItemStatusUpdate&)>;

    /*
      void onItemTypeUpdated(
                const IVIItemTypeStatusUpdate& update)
    */
    using OnItemTypeUpdated
            = function<void(
                const IVIItemTypeStatusUpdate&)>;

    /*
      void onOrderUpdated(
                const IVIOrderStateUpdate& update)
    */
    using OnOrderUpdated
            = function<void(
                const IVIOrderStatusUpdate&)>;

    /*
      void onPlayerUpdated(
                const IVIOrderStatusUpdate& update)
    */
    using OnPlayerUpdated
            = function<void(
                const IVIPlayerStatusUpdate&)>;
}

#endif // __IVI_EXECUTOR_H__