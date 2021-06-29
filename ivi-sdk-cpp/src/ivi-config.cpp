#include "ivi/ivi-config.h"
#include "ivi/ivi-util.h"
#include "grpcpp/grpcpp.h"

#include <map>
#include <utility>

namespace ivi
{

IVIConfigurationPtr IVIConfiguration::DefaultConfiguration(const string& environmentId, const string& apiKey, const string& host)
{
    return make_shared<IVIConfiguration>(IVIConfiguration
    {
         environmentId
        ,apiKey
        ,host
        ,0
        ,2
        ,10
        ,true
    });
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
    const grpc::ChannelArguments& args,
    int32_t connectionTimeoutSecs /*= 10*/)
{
    IVI_CHECK(configuration.apiKey.size() > 0);
    IVI_CHECK(configuration.environmentId.size() > 0);
    IVI_CHECK(configuration.host.size() > 0);

    IVI_LOG_VERBOSE("Creating channel to: " + configuration.host);

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
        IVI_EXIT_FAILURE();
    }

    return make_shared<IVIConnection>(
            IVIConnection{
                channel,
                make_shared<grpc::CompletionQueue>(),
                make_shared<grpc::CompletionQueue>(),
            });
}

IVIConnectionPtr IVIConnection::InsecureConnection(const string& privateHost)
{
    IVI_CHECK(privateHost != IVIConfiguration::DefaultHost());
    return make_shared<IVIConnection>(
            IVIConnection{ 
                grpc::CreateChannel(privateHost, grpc::InsecureChannelCredentials()),
                make_shared<grpc::CompletionQueue>(),
                make_shared<grpc::CompletionQueue>()
            });
}

}
