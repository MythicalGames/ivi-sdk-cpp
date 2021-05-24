#ifndef __IVI_CLIENT_MGR_H__
#define __IVI_CLIENT_MGR_H__

#include "ivi/ivi-client.h"

namespace ivi
{
    class IVI_SDK_API IVIClientManager
        : private NonCopyable<IVIClientManager>
    {
    public:

        const IVIConfiguration&     GetConfig() const;



    protected:

                                    IVIClientManager(
                                        const IVIConfigurationPtr& configuration,
                                        const IVIConnectionPtr& connection);

        IVIConfigurationPtr         m_configuration;

        IVIConnectionPtr            m_connection;
    };

    /*
    * GENERAL USE
    * IVIClientManagerAsync is the root object for managing the various asynchronous IVI clients.
    * It is not thread-safe, nor are its children: either allocate 1 instance per thread, or
    * share a single instance safely using your favorite concurrent-safety mechanism.
    *
    * Poll() must regularly be called to poll for and process responses and fire off callbacks
    * (event-loop pattern).
    * 
    * Poll() attempts to be fault-tolerant but may stall the thread for limited fixed time
    * while attempting to recover from an error.
    *
    * ADVANCED OPTION
    * The IVIConfiguration::autoconfirmStreamUpdates boolean controls whether the stream
    * clients automatically send receipt-confirmation messages over the unary queue.
    * If you set it to false, some critical semantics change:
    *   (1) You must explicitly call the stream client's Confirm functions yourself.
    *   (2) You may call the PollStream() and PollUnary() methods from separate "Reader" threads,
    *       and the callbacks will execute from those separate threads.  You may also make API
    *       requests (including stream Confirm) from a 3rd "Writer" thread.
    *   (3) If you run these methods from separate threads, you are responsible for marshaling
    *       the data from from your stream reader thread to your unary writer thread to call Confirm.
    *       Note: IVI does not currently have any stream write RPC calls aside from stream
    *       initialization, so there is currently no need for stream writer thread.
    *
    * SELF-MANAGEMENT OPTION
    * You are also free to bypass IVIClientManager / Poll*() and use and maintain the various clients
    * yourself with your own semantics, in which case you will want to familiarize yourself
    * with underlying gRPC mechanisms
    * https://github.com/grpc/grpc/blob/master/doc/core/grpc-client-server-polling-engine-usage.md
    *
    * Also be aware that if you decide to bypass the IVIClientManager and manage the clients yourself,
    * the AsyncCallback "tags" for Unary clients are allocated per-call and must be deleted (or you
    * will leak memory), whereas the Stream client tags are not.
    */

    struct IVI_SDK_API IVIStreamCallbacks
    {
        OnItemUpdated               onItemUpdated;
        OnItemTypeUpdated           onItemTypeUpdated;
        OnOrderUpdated              onOrderUpdated;
        OnPlayerUpdated             onPlayerUpdated;
    };

    class IVI_SDK_API IVIClientManagerAsync
        : private IVIClientManager
    {
    public:

                                    // Default, easy-to-use constructor
                                    IVIClientManagerAsync(
                                        const IVIConfigurationPtr& configuration,
                                        const IVIStreamCallbacks& callbacks);
                                    // If you wish to specify your own configuration to the underlying gRPC layer
                                    IVIClientManagerAsync(
                                        const IVIConfigurationPtr& configuration,
                                        const grpc::ChannelArguments& channelArgs,
                                        const IVIStreamCallbacks& callbacks);
                                    // You probably don't want to use this constructor without understanding deep 
                                    // grpc::CompletionQueue IO & threading semantics - you have been warned
                                    IVIClientManagerAsync(
                                        const IVIConfigurationPtr& configuration,
                                        const IVIConnectionPtr& connection,
                                        const IVIStreamCallbacks& callbacks);

        virtual                     ~IVIClientManagerAsync();


        using                       IVIClientManager::GetConfig;

        // Simple polling call for typical autoconfirmStreamUpdates = true
        // Calls both PollStreams and PollUnary and tries to auto-recover from errors.
        // Returns false if an unrecoverable error was encountered and this instance
        // should be discarded (ie when the underlying gRPC channel fails).
        bool                        Poll();

        // See documentation on autoconfirmStreamUpdates = false
        // Returns true if there was a problem necessitating a teardown; Reinit may be called if the connection channel is still viable
        bool                        PollStream();

        // See documentation on autoconfirmStreamUpdates = false
        // Returns true if there was a problem necessitating a teardown; Reinit may be called if the connection channel is still viable
        bool                        PollUnary();

        void                        ReinitializeStream();

        void                        ReinitializeUnary();

        IVIItemClientAsync&         ItemClient();

        IVIItemTypeClientAsync&     ItemTypeClient();

        IVIPaymentClientAsync&      PaymentClient();

        IVIPlayerClientAsync&       PlayerClient();

        IVIItemStreamClient&        ItemStreamClient();

        IVIItemTypeStreamClient&    ItemTypeStreamClient();

        IVIOrderStreamClient&       OrderStreamClient();

        IVIPlayerStreamClient&      PlayerStreamClient();

    private:

        template<bool Unary>
        bool                        Poll();

        template<class TUnaryClient>
        void                        ReinitializeUnary(TUnaryClient& client);

        template<class TStreamClient>
        void                        ReinitializeStream(TStreamClient& client);

        void                        FinishStream();

        bool                        IsStreamFinished();

        IVIItemClientAsync          m_itemClientAsync;

        IVIItemTypeClientAsync      m_itemTypeClientAsync;

        IVIPaymentClientAsync       m_paymentClientAsync;

        IVIPlayerClientAsync        m_playerClientAsync;

        IVIItemStreamClient         m_itemStreamClient;

        IVIItemTypeStreamClient     m_itemTypeStreamClient;

        IVIOrderStreamClient        m_orderStreamClient;

        IVIPlayerStreamClient       m_playerStreamClient;

        bool                        m_terminating : 1;
    };

    /*
    * For utilizing the synchronous clients.  
    * NOT suggested for high-throughput, high-performance use.
    * Does not handle the various IVI data streams.
    */
    class IVI_SDK_API IVIClientManagerSync
        : private IVIClientManager
    {
    public:
                                    // Default, easy-to-use constructor
                                    IVIClientManagerSync(
                                        const IVIConfigurationPtr& configuration);
                                    // If you wish to specify your own configuration to the underlying gRPC layer
                                    IVIClientManagerSync(
                                        const IVIConfigurationPtr& configuration,
                                        const grpc::ChannelArguments& channelArgs);
                                    // You probably don't want to use this constructor without understanding deep 
                                    // grpc::CompletionQueue IO & threading semantics - you have been warned
                                    IVIClientManagerSync(
                                        const IVIConfigurationPtr& configuration,
                                        const IVIConnectionPtr& connection);

        using                       IVIClientManager::GetConfig;
        
        IVIItemClient&              ItemClient();

        IVIItemTypeClient&          ItemTypeClient();

        IVIPaymentClient&           PaymentClient();
        
        IVIPlayerClient&            PlayerClient();

    private:

        IVIItemClient               m_itemClient;

        IVIItemTypeClient           m_itemTypeClient;

        IVIPaymentClient            m_paymentClient;

        IVIPlayerClient             m_playerClient;
    };

}

#endif // __IVI_CLIENT_MGR_H__
