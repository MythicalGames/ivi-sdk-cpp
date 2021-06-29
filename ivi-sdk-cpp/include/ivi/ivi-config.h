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

        // IVIClientManagerAsync connection management settings
        uint32_t                                defaultTimeoutSecs;         // Amount of time to block on message-receive polling
        uint32_t                                errorTimeoutSecs;           // Amount of time to block on each message-receive polling when in auto-recovery
        uint32_t                                errorLoopMax;               // Number of times to poll message-receive when in auto-recovery, keep at >= 2
        bool                                    autoconfirmStreamUpdates;   // Affects threading semantics - see IVIClientManager for explanation

        static constexpr const char* DefaultHost() { return "sdk-api.iviengine.com:443"; }

        static IVIConfigurationPtr              DefaultConfiguration(
                                                    const string& environmentId, 
                                                    const string& apiKey,
                                                    const string& host = DefaultHost());
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
                                                    const grpc::ChannelArguments& args,
                                                    int32_t connectionTimeoutSecs = 10);
        static IVIConnectionPtr                 InsecureConnection(
                                                    const string& privateHost);
    };
}

#endif //__IVI_CONFIG_H__
