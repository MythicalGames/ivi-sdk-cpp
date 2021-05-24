#include "ivi/ivi-config.h"
#include "ivi/ivi-util.h"
#include "grpcpp/grpcpp.h"

#include <utility>

namespace ivi
{

IVIConfiguration IVIConfiguration::DefaultConfiguration(const string& environmentId, const string& apiKey)
{
    return
    {
         environmentId
        ,apiKey
        ,DefaultHost()
        ,true
    };
}

constexpr const char* IVIConfiguration::DefaultHost()
{
    return "sdk-api.iviengine.com:443";
}

class IVIApiKeyCredentials final
    : public grpc::MetadataCredentialsPlugin
{
public:
    IVIApiKeyCredentials(const grpc::string& apiKey)
        : m_apiKey(apiKey)
    {
    }

    grpc::Status GetMetadata(
        grpc::string_ref service_url,
        grpc::string_ref method_name,
        const grpc::AuthContext& channel_auth_context,
        std::multimap<grpc::string, grpc::string>* metadata) override
    {
        metadata->insert(std::make_pair("api-key", m_apiKey));
        return grpc::Status::OK;
    }

private:
    grpc::string      m_apiKey;
};

constexpr int32_t IVIConnection::DefaultKeepAliveMS()
{
    return 30 * 1000;
}

grpc::ChannelArguments IVIConnection::DefaultChannelArguments()
{
    grpc::ChannelArguments args;
    args.SetInt(GRPC_ARG_KEEPALIVE_TIME_MS, DefaultKeepAliveMS());
    return args;
}

IVIConnectionPtr IVIConnection::DefaultConnection(
    const IVIConfiguration& configuration)
{
    return DefaultConnection(configuration, DefaultChannelArguments());
}

IVIConnectionPtr IVIConnection::DefaultConnection(
    const IVIConfiguration& configuration, 
    const grpc::ChannelArguments& args)
{
    IVI_CHECK(configuration.apiKey.size() > 0);
    IVI_CHECK(configuration.environmentId.size() > 0);
    IVI_CHECK(configuration.host.size() > 0);

    IVI_LOG_VERBOSE("Creating channel to: " + configuration.host);

    const int32_t connectionTimeoutSecs = 10;
    ChannelPtr channel(
        grpc::CreateCustomChannel(
            configuration.host,
            grpc::CompositeChannelCredentials(
                grpc::SslCredentials(grpc::SslCredentialsOptions()),
                grpc::MetadataCredentialsFromPlugin(
                    std::unique_ptr<grpc::MetadataCredentialsPlugin>(
                        new IVIApiKeyCredentials(configuration.apiKey)))),
            args));

    if (channel->WaitForConnected(gpr_timespec{ connectionTimeoutSecs, 0, GPR_TIMESPAN }))
    {
        IVI_LOG_INFO("Connected to: " + configuration.host);
    }
    else
    {
        IVI_LOG_CRITICAL("Failed to connect to: " + configuration.host);
    }

    return make_shared<IVIConnection>(
            IVIConnection{
                channel,
                make_shared<grpc::CompletionQueue>(),
                make_shared<grpc::CompletionQueue>()
            });
}

}
