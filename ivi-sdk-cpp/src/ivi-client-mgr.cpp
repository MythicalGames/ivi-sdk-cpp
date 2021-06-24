#include "ivi/ivi-client-mgr.h"
#include "ivi/ivi-util.h"

#include "grpcpp/grpcpp.h"

namespace ivi
{
    const IVIConfiguration& IVIClientManager::GetConfig() const
    {
        return *m_configuration;
    }

    IVIClientManager::IVIClientManager(
        const IVIConfigurationPtr& configuration, 
        const IVIConnectionPtr& connection)
        : m_configuration(configuration)
        , m_connection(connection)
    {

    }

    IVIClientManagerAsync::IVIClientManagerAsync(
        const IVIConfigurationPtr& configuration,
        const IVIStreamCallbacks& callbacks)
        : IVIClientManagerAsync(
            configuration, 
            IVIConnection::DefaultChannelArguments(),
            callbacks)
    {
    }

    IVIClientManagerAsync::IVIClientManagerAsync(
        const IVIConfigurationPtr& configuration,
        const grpc::ChannelArguments& channelArgs,
        const IVIStreamCallbacks& callbacks)
        : IVIClientManagerAsync(
            configuration, 
            IVIConnection::DefaultConnection(
                *configuration, 
                channelArgs),
            callbacks)
    {
    }

    IVIClientManagerAsync::IVIClientManagerAsync(
        const IVIConfigurationPtr& configuration,
        const IVIConnectionPtr& connection,
        const IVIStreamCallbacks& callbacks)
        : IVIClientManager(configuration, connection)
        , m_itemClientAsync(m_configuration, m_connection)
        , m_itemTypeClientAsync(m_configuration, m_connection)
        , m_orderClientAsync(m_configuration, m_connection)
        , m_paymentClientAsync(m_configuration, m_connection)
        , m_playerClientAsync(m_configuration, m_connection)
        , m_itemStreamClient(m_configuration, m_connection, callbacks.onItemUpdated)
        , m_itemTypeStreamClient(m_configuration, m_connection, callbacks.onItemTypeUpdated)
        , m_orderStreamClient(m_configuration, m_connection, callbacks.onOrderUpdated)
        , m_playerStreamClient(m_configuration, m_connection, callbacks.onPlayerUpdated)
    {
        IVI_LOG_FUNC_TRIVIAL();
        IVI_CHECK(&m_connection->unaryQueue != &m_connection->streamQueue); // sanity check
        if (m_configuration->errorLoopMax < 2)
        {
            IVI_LOG_CRITICAL("errorLoopMax < 2, IVIClientManagerAsync autorecovery may not work correctly and memory may leak");
        }
    }

    IVIClientManagerAsync::~IVIClientManagerAsync()
    {
        IVI_LOG_FUNC();
        IVI_LOG_INFO("IVIClientManager attempting graceful shutdown");

        // Graceful immediate teardown is a bit ugly
        auto drainQueue = [&](bool unary)
        {
            const CompletionQueuePtr& queue(unary ? m_connection->unaryQueue : m_connection->streamQueue);
            queue->Shutdown();
            grpc::CompletionQueue::NextStatus nextStatus;
            do 
            {
                void* tag = nullptr;
                bool ok = true;
                nextStatus =
                    queue->AsyncNext(&tag, &ok, gpr_timespec{ 0, 0, GPR_TIMESPAN });

                if (unary && tag != nullptr)
                {
                    AsyncCallback* cb = static_cast<AsyncCallback*>(tag);
                    delete cb;
                }
            } while (nextStatus == grpc::CompletionQueue::GOT_EVENT);

            if (nextStatus != grpc::CompletionQueue::SHUTDOWN)
            {
                IVI_LOG_CRITICAL("IVIClientManager did not shutdown gracefully");
            }
        };

        drainQueue(true);
        FinishStream(); // Calls Finish
        FinishStream(); // Calls TryCancel
        drainQueue(false);
    }

    bool IVIClientManagerAsync::Poll()
    {
        IVI_CHECK(m_configuration->autoconfirmStreamUpdates);
        
        bool unaryShutdown = PollUnary();
        bool streamShutdown = PollStream();

        // Automatic failure recovery attempt
        if (unaryShutdown || streamShutdown)
        {
            const grpc_connectivity_state channelState(m_connection->channel->GetState(false));
            if (channelState == GRPC_CHANNEL_SHUTDOWN)
            {
                IVI_LOG_CRITICAL("IVIClientManager connection encountered UNRECOVERABLE failure");
                return false;
            }

            if (unaryShutdown)
            {
                IVI_LOG_INFO("IVIClientManager reinitializing unary clients");
                ReinitializeUnary();
            }

            if (streamShutdown)
            {
                IVI_LOG_INFO("IVIClientManager reinitializing stream clients");
                ReinitializeStream();
            }
        }

        return true;
    }

    bool IVIClientManagerAsync::PollStream()
    {
        IVI_LOG_FUNC();
        return Poll<false>();
    }

    bool IVIClientManagerAsync::PollUnary()
    {
        IVI_LOG_FUNC();
        return Poll<true>();
    }

    template<bool Unary>
    bool IVIClientManagerAsync::Poll()
    {
        const CompletionQueuePtr& queue(Unary ? m_connection->unaryQueue : m_connection->streamQueue);
        const char* queueName(Unary ? "unary" : "stream");
        grpc::CompletionQueue::NextStatus nextStatus;

        auto processQueue = [&](int32_t waitS) -> bool
        {
            bool callShutdown = false;
            do
            {
                void* tag = nullptr;
                bool ok = true;
                nextStatus =
                    queue->AsyncNext(&tag, &ok, gpr_timespec{ waitS, 0, GPR_TIMESPAN });

                if (!ok)
                {
                    callShutdown = true;
                }

                if (nextStatus == grpc::CompletionQueue::GOT_EVENT && tag != nullptr)
                {
                    AsyncCallback* cb = static_cast<AsyncCallback*>(tag);
                    (*cb)(ok);

                    if (Unary)
                        delete cb;
                }
            } while (nextStatus == grpc::CompletionQueue::GOT_EVENT);

            return callShutdown;
        };

        const bool callShutdown = processQueue(m_configuration->defaultTimeoutSecs);

        // gRPC has poorly-documented semantics for handling failed connections, 
        // Not making the right calls in the right order can cause an internal assert and abort the program
        if (callShutdown)
        {
            IVI_LOG_WARNING("IVIClientManager ", queueName, " queue got ok=false, will attempt SHUTDOWN and restart");

            const uint32_t timeout = m_configuration->errorTimeoutSecs;
            const uint32_t maxShutdownPolls = m_configuration->errorLoopMax;

            /* "there are no more messages to be received from the server 
             *  (this can be known implicitly by the calling code, or explicitly 
             *  from an earlier call to AsyncReaderInterface::Read that yielded a 
             *  failed result, e.g. cq->Next(&read_tag, &ok) filled in 'ok' with 'false')."
            */
            if (!Unary)
            {
                IVI_LOG_INFO("IVIClientManager ", queueName, " queue issuing Finish/Cancel");
                FinishStream();
            }

            processQueue(timeout);

            /* "This method must be called at some point if this completion queue is accessed 
             *  with Next or AsyncNext. Next will not return false until this method has been 
             *  called and all pending tags have been drained. (Likewise for AsyncNext returning 
             *  NextStatus::SHUTDOWN .) Only once either one of these methods does that (that is, 
             *  once the queue has been drained) can an instance of this class be destroyed. 
             *  Also note that applications must ensure that no work is enqueued on this completion 
             *  queue after this method is called." */
            IVI_LOG_INFO("IVIClientManager ", queueName, " issuing shutdown");
            queue->Shutdown();

            uint32_t pollCount = 0;
            while ((nextStatus != grpc::CompletionQueue::SHUTDOWN || !IsStreamFinished()) && pollCount < maxShutdownPolls)
            {
                IVI_LOG_INFO("IVIClientManager ", queueName, " post-shutdown draining...");
                processQueue(timeout);

                if (!Unary && ++pollCount == maxShutdownPolls / 2)
                {
                    IVI_LOG_INFO("IVIClientManager ", queueName, " queue issuing Finish/Cancel AGAIN");
                    FinishStream();
                }
            }

            if (pollCount >= maxShutdownPolls)
            {
                IVI_LOG_CRITICAL("IVIClientManager ", queueName, " SHUTDOWN did NOT complete gracefully, possible memory leak");
            }
            else
            {
                IVI_LOG_INFO("IVIClientManager ", queueName, " SHUTDOWN completed gracefully, clients Finished and queue drained");
            }
        }

        return callShutdown;
    }

    void IVIClientManagerAsync::FinishStream()
    {
        m_itemStreamClient.Finish();
        m_itemTypeStreamClient.Finish();
        m_orderStreamClient.Finish();
        m_playerStreamClient.Finish();
    }

    bool IVIClientManagerAsync::IsStreamFinished()
    {
        return      m_itemStreamClient.IsFinished()
                &&  m_itemTypeStreamClient.IsFinished()
                &&  m_orderStreamClient.IsFinished()
                &&  m_playerStreamClient.IsFinished();
    }

    template<class TUnaryClient>
    void IVIClientManagerAsync::ReinitializeUnary(TUnaryClient& client)
    {
        (&client)->~TUnaryClient();
        new (&client) TUnaryClient(m_configuration, m_connection);
    }

    void IVIClientManagerAsync::ReinitializeUnary()
    {
        m_connection->unaryQueue = make_shared<grpc::CompletionQueue>();
        ReinitializeUnary(m_itemClientAsync);
        ReinitializeUnary(m_itemTypeClientAsync);
        ReinitializeUnary(m_paymentClientAsync);
        ReinitializeUnary(m_playerClientAsync);
    }

    template<class TStreamClient>
    void IVIClientManagerAsync::ReinitializeStream(TStreamClient& client)
    {
        typename TStreamClient::CallbackT callback(client.GetCallback());
        (&client)->~TStreamClient();
        new (&client) TStreamClient(m_configuration, m_connection, move(callback));
    }

    void IVIClientManagerAsync::ReinitializeStream()
    {
        m_connection->streamQueue = make_shared<grpc::CompletionQueue>();
        ReinitializeStream(m_itemStreamClient);
        ReinitializeStream(m_itemTypeStreamClient);
        ReinitializeStream(m_orderStreamClient);
        ReinitializeStream(m_playerStreamClient);
    }

    IVIItemClientAsync& IVIClientManagerAsync::ItemClient()
    {
        return m_itemClientAsync;
    }

    IVIItemTypeClientAsync& IVIClientManagerAsync::ItemTypeClient()
    {
        return m_itemTypeClientAsync;
    }

    IVIOrderClientAsync& IVIClientManagerAsync::OrderClient()
    {
        return m_orderClientAsync;
    }

    IVIPaymentClientAsync& IVIClientManagerAsync::PaymentClient()
    {
        return m_paymentClientAsync;
    }

    IVIPlayerClientAsync& IVIClientManagerAsync::PlayerClient()
    {
        return m_playerClientAsync;
    }

    IVIItemStreamClient& IVIClientManagerAsync::ItemStreamClient()
    {
        return m_itemStreamClient;
    }

    IVIItemTypeStreamClient& IVIClientManagerAsync::ItemTypeStreamClient()
    {
        return m_itemTypeStreamClient;
    }

    IVIOrderStreamClient& IVIClientManagerAsync::OrderStreamClient()
    {
        return m_orderStreamClient;
    }

    IVIPlayerStreamClient& IVIClientManagerAsync::PlayerStreamClient()
    {
        return m_playerStreamClient;
    }

    IVIClientManagerSync::IVIClientManagerSync(
        const IVIConfigurationPtr& configuration)
        : IVIClientManagerSync(
            configuration,
            IVIConnection::DefaultChannelArguments())
    {
    }

    IVIClientManagerSync::IVIClientManagerSync(
        const IVIConfigurationPtr& configuration, 
        const grpc::ChannelArguments& channelArgs)
        : IVIClientManagerSync(
            configuration,
            IVIConnection::DefaultConnection(
                *configuration,
                channelArgs))
    {
    }

    IVIClientManagerSync::IVIClientManagerSync(
        const IVIConfigurationPtr& configuration, 
        const IVIConnectionPtr& connection)
        : IVIClientManager(configuration, connection)
        , m_itemClient(m_configuration, m_connection)
        , m_itemTypeClient(m_configuration, m_connection)
        , m_orderClient(m_configuration, m_connection)
        , m_paymentClient(m_configuration, m_connection)
        , m_playerClient(m_configuration, m_connection)
    {
    }

    IVIItemClient& IVIClientManagerSync::ItemClient()
    {
        return m_itemClient;
    }

    IVIItemTypeClient& IVIClientManagerSync::ItemTypeClient()
    {
        return m_itemTypeClient;
    }

    IVIOrderClient& IVIClientManagerSync::OrderClient()
    {
        return m_orderClient;
    }

    IVIPaymentClient& IVIClientManagerSync::PaymentClient()
    {
        return m_paymentClient;
    }

    IVIPlayerClient& IVIClientManagerSync::PlayerClient()
    {
        return m_playerClient;
    }

}