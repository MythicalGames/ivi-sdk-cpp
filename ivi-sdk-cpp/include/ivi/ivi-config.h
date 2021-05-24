#ifndef __IVI_CONFIG_H__
#define __IVI_CONFIG_H__

#include "ivi/ivi-sdk.h"
#include "ivi/ivi-types.h"

namespace ivi
{

    struct IVI_SDK_API IVIConfiguration
    {
        string                                  environmentId;
        string                                  apiKey;
        string                                  host;
        bool                                    autoconfirmStreamUpdates = true;   // affects threading semantics - see IVIClientManager for explanation

        static IVIConfiguration                 DefaultConfiguration(
                                                    const string& environmentId, 
                                                    const string& apiKey);
        static constexpr const char*            DefaultHost();
    };

    struct IVI_SDK_API IVIConnection
    {
        // Represents the underlying connection based on grpc::ChannelArguments
        ChannelPtr                              channel;
        
        // Underlying stream rpc tags and unary rpc tags have different semantics, easiest to just process them separately
        CompletionQueuePtr                      streamQueue;
        CompletionQueuePtr                      unaryQueue;

        static constexpr int32_t                DefaultKeepAliveMS();
        static grpc::ChannelArguments           DefaultChannelArguments();
        static IVIConnectionPtr                 DefaultConnection(
                                                    const IVIConfiguration& configuration);
        static IVIConnectionPtr                 DefaultConnection(
                                                    const IVIConfiguration& configuration,
                                                    const grpc::ChannelArguments& args);
    };
}

#endif //__IVI_CONFIG_H__
