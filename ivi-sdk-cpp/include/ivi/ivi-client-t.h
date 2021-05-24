#ifndef __IVI_CLIENT_T_H__
#define __IVI_CLIENT_T_H__

#include "ivi/ivi-sdk.h"
#include "ivi/ivi-types.h"

/*
* Class template declarations for the various client types.  Kept here
* to declutter ivi-client.h.  Template implementations not public, all necessary
* instantiations are in ivi-client.cpp.  This header is primarily
* useful for custom client manager implementations, not general use.
*/

namespace ivi
{
    template<typename TPayload = void>
    class IVIResultT
        : public tuple<IVIResultStatus, TPayload>
    {
    public:
        using PayloadT              = TPayload;
        using tuple<IVIResultStatus, PayloadT>::tuple;

        IVIResultT(IVIResultStatus status)
            : tuple<IVIResultStatus, PayloadT>::tuple(status, PayloadT())
        {
        }

        IVIResultStatus             Status() const      { return get<0>(*this); }
        const PayloadT&             Payload() const     { return get<1>(*this); }
        PayloadT&                   Payload()           { return get<1>(*this); }
        bool                        Success() const     { return Status() == IVIResultStatus::SUCCESS; }
    };

    template<>
    class IVIResultT<void>
        : public tuple<IVIResultStatus>
    {
    public:
        using                       tuple<IVIResultStatus>::tuple;
        IVIResultStatus             Status() const      { return get<0>(*this); }
        bool                        Success() const     { return Status() == IVIResultStatus::SUCCESS; }
    };

    class IVI_SDK_API IVIClient
        : private NonCopyable<IVIClient>
    {

    public:

        const IVIConfiguration&     GetConfig() const;
        const IVIConfigurationPtr&  GetConfigPtr() const;

    protected:

                                    IVIClient(
                                        const IVIConfigurationPtr& configuration,
                                        const IVIConnectionPtr& conn);
        virtual                     ~IVIClient() = default;

        IVIConnectionPtr&           Connection();

    private:

        IVIConfigurationPtr         m_configuration;

        IVIConnectionPtr            m_connection;
    };

    template<typename TService>
    class IVI_SDK_API IVIClientT
        : public IVIClient
    {
    public:
        using ServiceT              = TService;

                                    IVIClientT(
                                        const IVIConfigurationPtr& configuration,
                                        const IVIConnectionPtr& conn);

        virtual                     ~IVIClientT();

    protected:

        template<class TStub>
        TStub*                      Stub();

        template<
            typename TResult,
            typename TResponse,
            typename TRequest,
            typename TRequestCall,
            typename TResponseParser
        >
        TResult                     CallUnary(
                                        TRequest&& request,
                                        TRequestCall&& call,
                                        TResponseParser&& parser);

        template<
            typename TResult,
            typename TResponse,
            typename TRequest,
            typename TRequestCall,
            typename TResponseParser,
            typename TResponseCallback
        >
        void                        CallUnaryAsync(
                                        TRequest&& request,
                                        TRequestCall&& call,
                                        TResponseParser&& parser,
                                        TResponseCallback&& callback);

        static bool                 CheckOkUnaryAsync(
                                        bool ok, 
                                        const grpc::ClientContext& context,
                                        const grpc::Status& status);
        
        static void                 LogFailure(
                                        const char* message, 
                                        const grpc::ClientContext& context, 
                                        const grpc::Status& status);

    private:
        void*                       m_stubPtr;
    };

    template<typename TStreamClientTraits>
    class IVI_SDK_API IVIStreamClientT
        : public IVIClientT<typename TStreamClientTraits::ServiceType>
    {
    public:

        virtual                     ~IVIStreamClientT();

        using TraitsT               = TStreamClientTraits;
        using StreamClientT         = typename TraitsT::StreamClient;
        using CallbackT             = typename TraitsT::CallbackType;
        using MessageT              = typename TraitsT::MessageType;
        using ServiceT              = typename TraitsT::ServiceType;
        using Base                  = IVIClientT<ServiceT>;

        CallbackT                   GetCallback() const;

        void                        Finish();

        bool                        IsFinished();

    protected:

        template<
            typename TSubscriber,
            typename TResponseHandler,
            typename TConfirmer
        >        
                                    IVIStreamClientT(
                                        const IVIConfigurationPtr& configuration,
                                        const IVIConnectionPtr& conn,
                                        const CallbackT& callback,
                                        TSubscriber&& subscribe,
                                        TResponseHandler&& onResponse,
                                        TConfirmer&& sendConfirm);
        
        template<
            typename TConfirmRequestCreator,
            typename TConfirmRequestFunc
        >
        void                        Confirm(
                                        TConfirmRequestCreator&& requestCreator, 
                                        TConfirmRequestFunc&& confirmRequestFunc);

        const MessageT&             CurrentMessage() const;

        template<typename... Args>  
        void                        OnCallback(Args&&... args);

    private:

        template<typename TSubscriber>
        void                        Subscribe(TSubscriber&& subscribe);

        void                        ReadNext();

        template<
            typename TResponseHandler,
            typename TConfirmer
        >
        void                        ProcessNext(
                                        TResponseHandler&& onResponse,
                                        TConfirmer&& sendConfirm,
                                        bool ok);

        CallbackT                   m_callback;

        // Hiding implementation details from class layout to prevent header pollution
        struct                      StreamState;
        using StreamStatePtr        = unique_ptr< StreamState >;
        StreamStatePtr              m_state;

        AsyncCallback               m_streamAdapter;
    };
}

#endif // __IVI_CLIENT_T_H__
