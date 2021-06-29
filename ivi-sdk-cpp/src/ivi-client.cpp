#include "ivi/ivi-client.h"
#include "ivi/ivi-model.h"
#include "ivi/ivi-util.h"

#include "grpcpp/grpcpp.h"
#include "grpc/support/log.h"

#include "ivi/generated/common/common.grpc.pb.h"
#include "ivi/generated/api/item/definition.grpc.pb.h"
#include "ivi/generated/api/item/rpc.grpc.pb.h"
#include "ivi/generated/api/itemtype/definition.grpc.pb.h"
#include "ivi/generated/api/itemtype/rpc.grpc.pb.h"
#include "ivi/generated/api/order/rpc.grpc.pb.h"
#include "ivi/generated/api/payment/rpc.grpc.pb.h"
#include "ivi/generated/api/player/rpc.grpc.pb.h"
#include "ivi/generated/streams/common.grpc.pb.h"
#include "ivi/generated/streams/item/stream.grpc.pb.h"
#include "ivi/generated/streams/itemtype/stream.grpc.pb.h"
#include "ivi/generated/streams/order/stream.grpc.pb.h"
#include "ivi/generated/streams/player/stream.grpc.pb.h"

#include <type_traits>

namespace ivi
{
    //////////////////////////////////////////////////////////////////////////
    // Free helper functions
    //////////////////////////////////////////////////////////////////////////

    bool TryGetHttpCode(const grpc::ClientContext& context, int& value)
    {
        static const string HTTP_CODE_KEY("httpcode");

        const auto& trailers(context.GetServerTrailingMetadata());

        auto httpCodeKey(trailers.find(HTTP_CODE_KEY));
        if (httpCodeKey != trailers.end())
        {
            // stoi will throw an exception and may abort the program.  
            // We want that if something has gone so wrong that HTTP codes are being sent as non-integers.
            value = stoi(httpCodeKey->second.data());
            return true;
        }
        return false;
    }

    IVIResultStatus TranslateHttpError(const grpc::ClientContext& context)
    {
        int httpCode;
        if(TryGetHttpCode(context, httpCode))
        {
            switch (httpCode)
            {
            case 400: //HTTP_BAD_REQUEST
                return IVIResultStatus::BAD_REQUEST;
            case 401: //HTTP_UNAUTHORIZED
                return IVIResultStatus::NOT_AUTHORIZED;
            case 403: //HTTP_FORBIDDEN
                return IVIResultStatus::FORBIDDEN;
            case 404: //HTTP_NOT_FOUND
                return IVIResultStatus::NOT_FOUND;
            case 409: //HTTP_CONFLICT
                return IVIResultStatus::CONFLICT;
            case 408: //HTTP_CLIENT_TIMEOUT
                return IVIResultStatus::TIMEOUT;
            case 422: //UNPROCESSABLE_ENTITY
                return IVIResultStatus::UNPROCESSABLE_ENTITY;
            default:
                return IVIResultStatus::UNKNOWN_ERROR;
            }
        }

        return IVIResultStatus::SUCCESS;
    }


    IVIResultStatus TranslateGrpcError(const grpc::ClientContext& context, const grpc::Status& status)
    {
        // "If grpc-status was provided, it must be used."
        // https://grpc.github.io/grpc/cpp/md_doc_http-grpc-status-mapping.html
        switch(status.error_code())
        {
        case grpc::INVALID_ARGUMENT:
            return IVIResultStatus::INVALID_ARGUMENT;
        case grpc::NOT_FOUND:
            return IVIResultStatus::NOT_FOUND;
        case grpc::PERMISSION_DENIED:
            return IVIResultStatus::PERMISSION_DENIED;
        case grpc::UNIMPLEMENTED:
            return IVIResultStatus::UNIMPLEMENTED;
        case grpc::UNAUTHENTICATED:
            return IVIResultStatus::UNAUTHENTICATED;
        case grpc::UNAVAILABLE:
            return IVIResultStatus::UNAVAILABLE;
        case grpc::RESOURCE_EXHAUSTED:
            return IVIResultStatus::RESOURCE_EXHAUSTED;
        case grpc::ABORTED:
            return IVIResultStatus::ABORTED;
        case grpc::DEADLINE_EXCEEDED:
        case grpc::FAILED_PRECONDITION:
        case grpc::OUT_OF_RANGE:
            return IVIResultStatus::BAD_REQUEST;
        case grpc::ALREADY_EXISTS:
            return IVIResultStatus::CONFLICT;
        case grpc::DATA_LOSS:
        case grpc::INTERNAL:
        case grpc::UNKNOWN:
            return IVIResultStatus::SERVER_ERROR;
        default:
            IVIResultStatus contextError(TranslateHttpError(context));
            if(contextError == IVIResultStatus::SUCCESS)
                return IVIResultStatus::UNKNOWN_ERROR;
            return contextError;
        }
    }

    //////////////////////////////////////////////////////////////////////////
    // Base IVIClient class, at minimum provides common virtual destructor
    //////////////////////////////////////////////////////////////////////////


    IVIClient::IVIClient(
        const IVIConfigurationPtr& configuration,
        const IVIConnectionPtr& conn)
        : m_configuration(configuration)
        , m_connection(conn)
    {
        IVI_CHECK(GetConfig().host.size() > 0);
        IVI_CHECK(GetConfig().apiKey.size() > 0);
        IVI_CHECK(GetConfig().environmentId.size() > 0);
    }
    
    IVIConnectionPtr& IVIClient::Connection()
    {
        return m_connection;
    }

    const ivi::IVIConfigurationPtr& IVIClient::GetConfigPtr() const
    {
        return m_configuration;
    }

    const IVIConfiguration& IVIClient::GetConfig() const
    {
        return *m_configuration;
    }

    //////////////////////////////////////////////////////////////////////////
    // IVIClientT class template 
    // Primary purposes is wrapping boilerplate gRPC semantics for
    // unary calls (sync & async)
    //////////////////////////////////////////////////////////////////////////

    template<typename TService>
    IVIClientT<TService>::IVIClientT(
        const IVIConfigurationPtr& configuration, 
        const IVIConnectionPtr& conn)
        : IVIClient(configuration, conn)
        , m_stubPtr(TService::NewStub(Connection()->channel).release())
    {
    }

    template<typename TService>
    ivi::IVIClientT<TService>::~IVIClientT()
    {
        delete Stub<typename ServiceT::Stub>();
    }

    // gRPC commits the sin of using public nested classes for their generated interfaces, 
    // which precludes forward declarations of those stubs in IVI's public headers.
    // To avoid the massive pollution of including gRPC headers in our public headers,
    // we're just going to manually manage a void*, instead of incurring
    // the expense and maintenance issues of a virtual interface & private 
    // implementation, or pImpl implementation.
    // Issue filed here: https://github.com/grpc/grpc/issues/25995
    template<typename TService>
    template<typename TStub>
    TStub* IVIClientT<TService>::Stub()
    {
        return static_cast<TStub*>(m_stubPtr);
    }

    /*static*/
    template<typename TService>
    void IVIClientT<TService>::LogFailure(
        const char* message, 
        const grpc::ClientContext& context, 
        const grpc::Status& status)
    {
#if IVI_LOGGING_LEVEL >= 3
        int httpCode = 0;
        TryGetHttpCode(context, httpCode);
        IVI_LOG_RPC_FAIL(ServiceT::service_full_name(), message,
            ": gRPCStatus=", status.error_code(),
            " HttpCode=", httpCode,
            " message=", status.error_message());
#endif // IVI_LOGGING_LEVEL >= 2
    }

    template<typename TResult, typename TResponseParser, typename TResponse,
    class = typename enable_if<!is_same<TResult,IVIResult>::value, TResult>::type>
    static TResult MakeSuccessResult(TResponseParser&& parser, const TResponse& response)
    {
        return { IVIResultStatus::SUCCESS,
                 parser(response) };
    }

    template<typename TResult, typename TResponseParser, typename TResponse,
    class = typename enable_if<is_same<TResult, IVIResult>::value, TResult>::type>
    static IVIResult MakeSuccessResult(TResponseParser&& /*parser*/, const TResponse& /*response*/)
    {
        return { IVIResultStatus::SUCCESS };
    }

    template<typename TService>
    template<
        typename TResult,
        typename TResponse,
        typename TRequest,
        typename TRequestCall,
        typename TResponseParser
    >
    TResult IVIClientT<TService>::CallUnary(
        TRequest&& request,
        TRequestCall&& call,
        TResponseParser&& parser)
    {
        IVI_LOG_NTRACE(ServiceT::service_full_name(), " Request: ", request.DebugString());

        request.set_environment_id(GetConfig().environmentId);
        grpc::ClientContext context;
        TResponse response;
        grpc::Status status( (Stub<typename ServiceT::Stub>()->*call)(&context, request, &response) ) ;

        if (status.ok())
        {
            IVI_LOG_NTRACE(ServiceT::service_full_name(), " Response: ", response.DebugString());
            return MakeSuccessResult<TResult>(parser, response);
        }
        else
        {
            LogFailure(" sync request FAILED", context, status);
            return { TranslateGrpcError(context, status) };
        }
    }

    template<typename TService>
    template<
        typename TResult,
        typename TResponse,
        typename TRequest,
        typename TRequestCall,
        typename TResponseParser,
        typename TResponseCallback
    >
    void IVIClientT<TService>::CallUnaryAsync(
        TRequest&& request,
        TRequestCall&& call,
        TResponseParser&& parser,
        TResponseCallback&& callback)
    {
        IVI_LOG_NTRACE(ServiceT::service_full_name(), " Request: ", request.DebugString());
		IVI_CHECK(callback);
        request.set_environment_id(GetConfig().environmentId);

        struct AsyncState
        {
            grpc::ClientContext         context;
            TResponse                   response;
            grpc::Status                status;
            using ReaderPtr             = unique_ptr<grpc::ClientAsyncResponseReader<TResponse>>;
            ReaderPtr                   reader;
        };

        shared_ptr<AsyncState> asyncState(make_shared<AsyncState>());
        
        asyncState->reader = (Stub<typename ServiceT::Stub>()->*call)(
            &asyncState->context,
            request,
            Connection()->unaryQueue.get());

        asyncState->reader->Finish(
            &asyncState->response,
            &asyncState->status,
            new AsyncCallback([asyncState, parser, callback](bool ok)
                {
                    if (CheckOkUnaryAsync(ok, asyncState->context, asyncState->status))
                    {
                        IVI_LOG_NTRACE(ServiceT::service_full_name(), " Response: ", asyncState->response.DebugString());
                        callback(MakeSuccessResult<TResult>(parser, asyncState->response));
                    }
                    else
                    {
                        callback({ TranslateGrpcError(asyncState->context, asyncState->status) });
                    }
                }
            )
        );
    }

    template<typename TService>
    /*static*/ bool IVIClientT<TService>::CheckOkUnaryAsync(
        bool ok,
        const grpc::ClientContext& context,
        const grpc::Status& status)
    {
        /*
         * "Client-side Read, Server-side Read, Client-side RecvInitialMetadata 
         *  (which is typically included in Read if not done explicitly): ok indicates 
         *  whether there is a valid message that got read. If not, you know that there 
         *  are certainly no more messages that can ever be read from this stream. For 
         *  the client-side operations, this only happens because the call is dead."
         */
        if (!ok)
        {
            LogFailure(" Async request received ok=FALSE", context, status);
            return false;
        }
        else if (!status.ok())
        {
            LogFailure(" Async request received ok=TRUE", context, status);
            return false;
        }

        return true;
    }

    //////////////////////////////////////////////////////////////////////////
    // IVIStreamClientT class template 
    // Wrap & hide gRPC's cryptic boilerplate semantics for async 
    // client-stream calls.
    // Also helps to avoid header & layout pollution from gRPC.
    //////////////////////////////////////////////////////////////////////////

    template<typename TStreamClientTraits>
    struct IVIStreamClientT<TStreamClientTraits>::StreamState
    {
        using TraitsT               = TStreamClientTraits;
        using ServiceT              = typename TraitsT::ServiceType;
        using MessageT              = typename TraitsT::MessageType;

        grpc::ClientContext         clientContext;
        grpc::Status                streamStatus;
        MessageT                    updateResponse;

        using UpdateReader          = unique_ptr< grpc::ClientAsyncReader<MessageT> >;
        UpdateReader                updateReader;

        AsyncCallback               startCallback = [this](bool ok)
        {
            /* "Client-side StartCall/RPC invocation: ok indicates that the RPC is going
            *   to go to the wire. If it is false, it not going to the wire. This would
            *   happen if the channel is either permanently broken or transiently broken
            *   but with the fail-fast option. (Note that async unary RPCs don't post a
            *   CQ tag at this point, nor do client-streaming or bidi-streaming RPCs that
            *   have the initial metadata corked option set.)"
            */
            startReceived = true;
            if (ok)
            {
                IVI_LOG_NTRACE(ServiceT::service_full_name(), " start success");
            }
            else
            {
                LogStreamFailure(" start FAILED");
            }
        };

        AsyncCallback               initMetadataCallback = [this](bool ok)
        {
            initMetadataReceived = true;
            if (ok)
            {
                IVI_LOG_NTRACE(ServiceT::service_full_name(), " metadata received");
            }
            else
            {
                LogStreamFailure(" metadata FAILED");
            }
        };

        AsyncCallback               finishCallback = [this](bool ok)
        {
            finishResponded = true;
            // "Client - side Finish : ok should always be true"
            if (ok)
            {
                IVI_LOG_VERBOSE(ServiceT::service_full_name(), " finish received");
            }
            else
            {
                LogStreamFailure(" Finish FAILED");
            }
        };

        // Subscribe request sent
        bool                        startReceived : 1;

        // Subscription initial metadata response recieved
        bool                        initMetadataReceived : 1;

        // Unsubscribe request sent
        bool                        finishCalled : 1;

        // Unsubscribe request received OR canceled, graceful teardown possible
        bool                        finishResponded : 1;
        
        StreamState()
            : startReceived(false)
            , initMetadataReceived(false)
            , finishCalled(false)
            , finishResponded(false)
        {}

        void LogStreamFailure(const char* message) const
        {
            IVIStreamClientT<TraitsT>::LogFailure(message, clientContext, streamStatus);
        }

    };

    template<typename TStreamClientTraits>
    template<
        typename TSubscriber,
        typename TConfirmer
    >
    IVIStreamClientT<TStreamClientTraits>::IVIStreamClientT(
        const IVIConfigurationPtr& configuration,
        const IVIConnectionPtr& conn,
        const CallbackT& callback,
        TSubscriber&& subscribe,
        TConfirmer&& sendConfirm)
        : Base::IVIClientT(configuration, conn)
        , m_callback(callback)
        , m_streamAdapter([this, sendConfirm](bool ok)
            {
                ProcessNext(sendConfirm, ok);
            })
        , m_state(new StreamState())
    {
        if (callback)
        {
            Subscribe(subscribe);
        }
        else
        {
            IVI_LOG_RPC_FAIL(ServiceT::service_full_name(), " not subscribed because no callback was associated");
            m_state->finishResponded = true;
        }
    }

    template<typename TStreamClientTraits>
    IVIStreamClientT<TStreamClientTraits>::~IVIStreamClientT()
    {
    }

    template<typename TStreamClientTraits>
    typename IVIStreamClientT<TStreamClientTraits>::CallbackT IVIStreamClientT<TStreamClientTraits>::GetCallback() const
    {
        return m_callback;
    }

    template<typename TStreamClientTraits>
    void IVIStreamClientT<TStreamClientTraits>::Finish()
    {
        if (m_state->finishResponded)
        {
            return;
        }

        // Weird gRPC semantics to ensure we pump out all tags when there's an error
        if (!m_state->initMetadataReceived || m_state->finishCalled)
        {
            m_state->clientContext.TryCancel();
            m_state->finishResponded = true;
            return;
        }

        if (!m_state->finishCalled)
        {
            IVI_LOG_VERBOSE(ServiceT::service_full_name(), " finish called");
            m_state->updateReader->Finish(&m_state->streamStatus, &m_state->finishCallback);
            m_state->finishCalled = true;
        }
    }

    template<typename TStreamClientTraits>
    bool IVIStreamClientT<TStreamClientTraits>::IsFinished()
    {
        return m_state->finishResponded;
    }

    template<typename TStreamClientTraits>
    template<typename TSubscriber>
    void IVIStreamClientT<TStreamClientTraits>::Subscribe(TSubscriber&& subscribe)
    {
        rpc::streams::Subscribe subscribeRequest;
        subscribeRequest.set_environment_id(Base::GetConfig().environmentId);

        IVI_LOG_NTRACE(ServiceT::service_full_name(), " Subscribe: ", subscribeRequest.DebugString());

        m_state->updateReader =
            (Base::template Stub<typename ServiceT::Stub>()->*subscribe)(
                &m_state->clientContext,
                move(subscribeRequest),
                Base::Connection()->streamQueue.get(),
                &m_state->startCallback);
        m_state->updateReader->ReadInitialMetadata(&m_state->initMetadataCallback);
        ReadNext(); // gRPC stream message has to be primed separately from init response and before the queue's Next
    }


    template<typename TStreamClientTraits>
    template<
        typename TConfirmRequestCreator,
        typename TConfirmRequestFunc
    >
    void IVIStreamClientT<TStreamClientTraits>::Confirm(
        TConfirmRequestCreator&& requestCreator, 
        TConfirmRequestFunc&& confirmRequestFunc)
    {
        Base::template CallUnaryAsync<IVIResult, google::protobuf::Empty>(
            requestCreator(),
            confirmRequestFunc,
            nullptr,
            [](const IVIResult& result)
            {
                if (result.Success())
                {
                    IVI_LOG_NTRACE(ServiceT::service_full_name(), " confirmation confirmed");
                }
                else
                {
                    IVI_LOG_WARNING(ServiceT::service_full_name(), " confirmation failed: ", static_cast<int32_t>(result.Status()));
                }
            });
    }

    template<typename TStreamClientTraits>
    const typename IVIStreamClientT<TStreamClientTraits>::MessageT& IVIStreamClientT<TStreamClientTraits>::CurrentMessage() const
    {
        return m_state->updateResponse;
    }

    template<typename TStreamClientTraits>
    void IVIStreamClientT<TStreamClientTraits>::OnCallback(const ParsedMessageT& message)
    {
        m_callback(message);
    }

    template<typename TStreamClientTraits>
    void IVIStreamClientT<TStreamClientTraits>::ReadNext()
    {
        m_state->updateReader->Read(
            &m_state->updateResponse,
            &m_streamAdapter);
    }

    template<typename TStreamClientTraits>
    template<
        typename TConfirmer
    >
    void IVIStreamClientT<TStreamClientTraits>::ProcessNext(
            TConfirmer&& sendConfirm,
            bool ok)
    {
        if(!ok)
        {
            m_state->LogStreamFailure(" ProcessNext FAILED");
            return;
        }

        IVI_LOG_NTRACE(ServiceT::service_full_name(), " Received: ", CurrentMessage().DebugString());

        OnCallback(ParsedMessageT::FromProto(CurrentMessage()));

        if (Base::GetConfig().autoconfirmStreamUpdates)
        {
            sendConfirm();
        }

        ReadNext();
    }

    //////////////////////////////////////////////////////////////////////////
    // Item Request Clients
    //////////////////////////////////////////////////////////////////////////

    template IVIClientT<IVIItemClient::ServiceT>::IVIClientT(
        const IVIConfigurationPtr& configuration,
        const IVIConnectionPtr& conn);
    IVIItemClient::~IVIItemClient() {}
    IVIItemClientAsync::~IVIItemClientAsync() {}

    static proto::api::item::IssueItemRequest MakeIssueItemRequest(
        const string& gameInventoryId,
        const string& playerId,
        const string& itemName,
        const string& gameItemTypeId,
        const BigDecimal& amountPaid,
        const string& currency,
        const IVIMetadata& metadata,
        const string& storeId,
        const string& orderId,
        const string& requestIp)
    {
        proto::api::item::IssueItemRequest request;
        request.set_game_inventory_id(gameInventoryId);
        request.set_player_id(playerId);
        request.set_item_name(itemName);
        request.set_game_item_type_id(gameItemTypeId);
        request.set_amount_paid(amountPaid);
        request.set_currency(currency);
        *request.mutable_metadata() = metadata.ToProto();
        request.set_store_id(storeId);
        request.set_order_id(orderId);
        request.set_request_ip(requestIp);
        return request;
    }

    template<class TResponse>
    struct ItemStateUpdateResponseParserT
    {
        string gameInventoryId;
        IVIItemStateChange operator()(const TResponse& response) const
        {
            return { gameInventoryId, response.tracking_id(), ECast(response.item_state()) };
        }
    };

    IVIResultItemStateChange IVIItemClient::IssueItem(
        const string& gameInventoryId,
        const string& playerId,
        const string& itemName,
        const string& gameItemTypeId,
        const BigDecimal& amountPaid,
        const string& currency,
        const IVIMetadata& metadata,
        const string& storeId,
        const string& orderId,
        const string& requestIp)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("IssueItem gameInventoryId=", gameInventoryId);

        using Response = proto::api::item::IssueItemStartedResponse;
        return CallUnary<IVIResultItemStateChange, Response>(
            MakeIssueItemRequest(gameInventoryId, playerId, itemName, gameItemTypeId, amountPaid, currency, metadata, storeId, orderId, requestIp),
            &ServiceT::Stub::IssueItem,
            ItemStateUpdateResponseParserT<Response>{ gameInventoryId });
    }

    void IVIItemClientAsync::IssueItem(
        const string& gameInventoryId,
        const string& playerId,
        const string& itemName,
        const string& gameItemTypeId,
        const BigDecimal& amountPaid,
        const string& currency,
        const IVIMetadata& metadata,
        const string& storeId,
        const string& orderId,
        const string& requestIp,
        const function<void(const IVIResultItemStateChange&)>& callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("IssueItem (async) gameInventoryId=", gameInventoryId);

        using Response = proto::api::item::IssueItemStartedResponse;
        CallUnaryAsync<IVIResultItemStateChange, Response>(
            MakeIssueItemRequest(gameInventoryId, playerId, itemName, gameItemTypeId, amountPaid, currency, metadata, storeId, orderId, requestIp),
            &ServiceT::Stub::AsyncIssueItem,
            ItemStateUpdateResponseParserT<Response>{ gameInventoryId },
            callback);
    }

    static proto::api::item::TransferItemRequest MakeTransferItemRequest(
        const string& gameInventoryId,
        const string& sourcePlayerId,
        const string& destPlayerId,
        const string& storeId)
    {
        proto::api::item::TransferItemRequest request;
        request.set_game_item_inventory_id(gameInventoryId);
        request.set_source_player_id(sourcePlayerId);
        request.set_destination_player_id(destPlayerId);
        request.set_store_id(storeId);
        return request;
    }

    IVIResultItemStateChange IVIItemClient::TransferItem(
        const string& gameInventoryId, 
        const string& sourcePlayerId, 
        const string& destPlayerId, 
        const string& storeId)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("TransferItem gameInventoryId=", gameInventoryId);

        using Response = proto::api::item::TransferItemStartedResponse;
        return CallUnary<IVIResultItemStateChange, Response>(
            MakeTransferItemRequest(gameInventoryId, sourcePlayerId, destPlayerId, storeId),
            &ServiceT::Stub::TransferItem, 
            ItemStateUpdateResponseParserT<Response>{ gameInventoryId });
    }

    void IVIItemClientAsync::TransferItem(
        const string& gameInventoryId,
        const string& sourcePlayerId,
        const string& destPlayerId,
        const string& storeId,
        const function<void(const IVIResultItemStateChange&)>& callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("TransferItem (async) gameInventoryId=", gameInventoryId);

        using Response = proto::api::item::TransferItemStartedResponse;
        CallUnaryAsync<IVIResultItemStateChange, Response>(
            MakeTransferItemRequest(gameInventoryId, sourcePlayerId, destPlayerId, storeId),
            &ServiceT::Stub::AsyncTransferItem,
            ItemStateUpdateResponseParserT<Response>{ gameInventoryId },
            callback);
    }

    static proto::api::item::BurnItemRequest MakeBurnItemRequest(
        const string& gameInventoryId)
    {
        proto::api::item::BurnItemRequest request;
        request.set_game_item_inventory_id(gameInventoryId);
        return request;
    }

    IVIResultItemStateChange IVIItemClient::BurnItem(
        const string& gameInventoryId)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("BurnItem gameInventoryId=", gameInventoryId);

        using Response = proto::api::item::BurnItemStartedResponse;
        return CallUnary<IVIResultItemStateChange, Response>(
            MakeBurnItemRequest(gameInventoryId),
            &ServiceT::Stub::BurnItem,
            ItemStateUpdateResponseParserT<Response>{ gameInventoryId });
    }

    void IVIItemClientAsync::BurnItem(
        const string& gameInventoryId,
        const function<void(const IVIResultItemStateChange&)>& callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("BurnItem (async) gameInventoryId=", gameInventoryId);

        using Response = proto::api::item::BurnItemStartedResponse;
        CallUnaryAsync<IVIResultItemStateChange, Response>(
            MakeBurnItemRequest(gameInventoryId),
            &ServiceT::Stub::AsyncBurnItem,
            ItemStateUpdateResponseParserT<Response>{ gameInventoryId },
            callback);
    }

    static proto::api::item::GetItemRequest MakeGetItemRequest(
        const string& gameInventoryId,
        bool history)
    {
        proto::api::item::GetItemRequest request;
        request.set_game_inventory_id(gameInventoryId);
        request.set_history(history);
        return request;
    }

    IVIResultItem IVIItemClient::GetItem(
        const string& gameInventoryId, 
        bool history)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("GetItem gameInventoryId=", gameInventoryId);

        using Response = proto::api::item::Item;
        return CallUnary<IVIResultItem, Response>(
            MakeGetItemRequest(gameInventoryId, history),
            &ServiceT::Stub::GetItem,
            &IVIItem::FromProto);
    }

    void IVIItemClientAsync::GetItem(
        const string& gameInventoryId,
        const function<void(const IVIResultItem&)>& callback)
    {
        GetItem(gameInventoryId, false, callback);
    }

    void IVIItemClientAsync::GetItem(
        const string& gameInventoryId,
        bool history,
        const function<void(const IVIResultItem&)>& callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("GetItem (async) gameInventoryId=", gameInventoryId);

        using Response = proto::api::item::Item;
        CallUnaryAsync<IVIResultItem, Response>(
            MakeGetItemRequest(gameInventoryId, history),
            &ServiceT::Stub::AsyncGetItem,
            &IVIItem::FromProto, 
            callback);
    }

    static proto::api::item::GetItemsRequest MakeGetItemsRequest(
        time_t createdTimestamp,
        int32_t pageSize,
        SortOrder sortOrder,
        Finalized finalized)
    {
        proto::api::item::GetItemsRequest request;
        request.set_created_timestamp(createdTimestamp);
        request.set_page_size(pageSize);
        request.set_sort_order(ECast(sortOrder));
        request.set_finalized(ECast(finalized));
        return request;
    }

    static IVIResultItemList::PayloadT ParseItems(const proto::api::item::Items& response)
    {
        list<IVIItem> outItems;
        transform(response.items().begin(), response.items().end(), back_inserter(outItems), &IVIItem::FromProto);
        return outItems;
    }

    IVIResultItemList IVIItemClient::GetItems(
        time_t createdTimestamp, 
        int32_t pageSize, 
        SortOrder sortOrder, 
        Finalized finalized)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("GetItems pageSize=", pageSize);

        using Response = proto::api::item::Items;
        return CallUnary<IVIResultItemList, Response>(
            MakeGetItemsRequest(createdTimestamp, pageSize, sortOrder, finalized),
            &ServiceT::Stub::GetItems,
            &ParseItems);
    }

    void IVIItemClientAsync::GetItems(
        time_t createdTimestamp,
        int32_t pageSize,
        SortOrder sortOrder,
        Finalized finalized,
        const function<void(const IVIResultItemList&)>& callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("GetItems (async) pageSize=", pageSize);

        using Response = proto::api::item::Items;
        return CallUnaryAsync<IVIResultItemList, Response>(
            MakeGetItemsRequest(createdTimestamp, pageSize, sortOrder, finalized),
            &ServiceT::Stub::AsyncGetItems,
            &ParseItems,
            callback);
    }

    proto::api::item::UpdateItemMetadataRequest MakeUpdateItemMetadataRequest(
        const string& gameInventoryId,
        const IVIMetadata& metadata)
    {
        proto::api::item::UpdateItemMetadataRequest request;
        auto protoMetadata(request.add_update_items());
        protoMetadata->set_game_inventory_id(gameInventoryId);
        *protoMetadata->mutable_metadata() = metadata.ToProto();
        return request;
    }

    IVIResult IVIItemClient::UpdateItemMetadata(
        const string& gameInventoryId, 
        const IVIMetadata& metadata)
    {

        return UpdateItemMetadata(MakeUpdateItemMetadataRequest(gameInventoryId, metadata));
    }

    proto::api::item::UpdateItemMetadataRequest MakeUpdateItemMetadataRequest(const IVIMetadataUpdateList& updates)
    {
        proto::api::item::UpdateItemMetadataRequest request;
        request.mutable_update_items()->Reserve(static_cast<int>(updates.size()));
        transform(updates.begin(), updates.end(), google::protobuf::RepeatedPtrFieldBackInserter(request.mutable_update_items()),
            [](const IVIMetadataUpdate& update) { return update.ToProto();  });
        return request;
    }

    IVIResult IVIItemClient::UpdateItemMetadata(
        const IVIMetadataUpdateList& updates)
    {
        return UpdateItemMetadata(MakeUpdateItemMetadataRequest(updates));
    }

    void IVIItemClientAsync::UpdateItemMetadata(
        const string& gameInventoryId, 
        const IVIMetadata& metadata,
        const function<void(const IVIResult&)>& callback)
    {
        UpdateItemMetadata(MakeUpdateItemMetadataRequest(gameInventoryId, metadata), callback);
    }

    void IVIItemClientAsync::UpdateItemMetadata(
        const IVIMetadataUpdateList& updates,
        const function<void(const IVIResult&)>& callback)
    {
        UpdateItemMetadata(MakeUpdateItemMetadataRequest(updates), callback);
    }

    IVIResult IVIItemClient::UpdateItemMetadata(
        proto::api::item::UpdateItemMetadataRequest updateRequest)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("UpdateItemMetadata request: ", updateRequest.update_items().size());

        using Response = proto::api::item::UpdateItemMetadataResponse;
        return CallUnary<IVIResult, Response>(
            move(updateRequest),
            &ServiceT::Stub::UpdateItemMetadata,
            nullptr);
    }

    void IVIItemClientAsync::UpdateItemMetadata(
        proto::api::item::UpdateItemMetadataRequest updateRequest,
        const function<void(const IVIResult&)>& callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("UpdateItemMetadata (async) request: ", updateRequest.update_items().size());

        using Response = proto::api::item::UpdateItemMetadataResponse;
        CallUnaryAsync<IVIResult, Response>(
            move(updateRequest),
            &ServiceT::Stub::AsyncUpdateItemMetadata,
            nullptr,
            callback);
    }

    //////////////////////////////////////////////////////////////////////////
    // ItemType request clients
    //////////////////////////////////////////////////////////////////////////

    template IVIClientT<IVIItemTypeClient::ServiceT>::IVIClientT(
        const IVIConfigurationPtr& configuration,
        const IVIConnectionPtr& conn);
    IVIItemTypeClient::~IVIItemTypeClient() {}
    IVIItemTypeClientAsync::~IVIItemTypeClientAsync() {}

    static IVIResultItemType ParseItemTypeListToElement(const IVIResultItemTypeList& result)
    {
        if (result.Success())
        {
            if (result.Payload().size() >= 1)
            {
                return { result.Status(), result.Payload().front() };
            }
            return { IVIResultStatus::NOT_FOUND };
        }
        return { result.Status() };
    }

    IVIResultItemType IVIItemTypeClient::GetItemType(const string& gameItemTypeId)
    {
        StringList strlist;
        strlist.push_back(gameItemTypeId);
        return ParseItemTypeListToElement(GetItemTypes(strlist));
    }

    void IVIItemTypeClientAsync::GetItemType(
        const string& gameItemTypeId,
        const function<void(const IVIResultItemType&)>& callback)
    {
        StringList strlist;
        strlist.push_back(gameItemTypeId);
        GetItemTypes(
            strlist, 
            [callback](const IVIResultItemTypeList& result)
            {
                callback(ParseItemTypeListToElement(result));
            });
    }

    static proto::api::itemtype::GetItemTypesRequest MakeGetItemTypesRequest(const StringList& gameItemTypeIds)
    {
        proto::api::itemtype::GetItemTypesRequest request;
        *request.mutable_game_item_type_ids() = { gameItemTypeIds.begin(), gameItemTypeIds.end() };
        return request;
    }

    static IVIResultItemTypeList::PayloadT ParseItemTypes(const proto::api::itemtype::ItemTypes& response)
    {
        list<IVIItemType> outItems;
        transform(response.item_types().begin(), response.item_types().end(), back_inserter(outItems), &IVIItemType::FromProto);
        return outItems;
    }

    IVIResultItemTypeList IVIItemTypeClient::GetItemTypes(const StringList& gameItemTypeIds)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("GetItemTypes request: ", gameItemTypeIds.size());

        using Response = proto::api::itemtype::ItemTypes;
        return CallUnary<IVIResultItemTypeList, Response>(
            MakeGetItemTypesRequest(gameItemTypeIds),
            &ServiceT::Stub::GetItemTypes,
            &ParseItemTypes);
    }

    void IVIItemTypeClientAsync::GetItemTypes(
        const StringList& gameItemTypeIds,
        const function<void(const IVIResultItemTypeList&)>& callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("GetItemTypes (async) request: ", gameItemTypeIds.size());

        using Response = proto::api::itemtype::ItemTypes;
        CallUnaryAsync<IVIResultItemTypeList, Response>(
            MakeGetItemTypesRequest(gameItemTypeIds),
            &ServiceT::Stub::AsyncGetItemTypes,
            &ParseItemTypes,
            callback);
    }

    static proto::api::itemtype::CreateItemTypeRequest MakeCreateItemTypeRequest(
        const string& gameItemTypeId,
        const string& tokenName,
        const string& category,
        int32_t maxSupply,
        int32_t issueTimeSpan,
        bool burnable,
        bool transferable,
        bool sellable,
        const UUIDList& agreementIds,
        const IVIMetadata& metadata)
    {
        proto::api::itemtype::CreateItemTypeRequest request;
        request.set_game_item_type_id(gameItemTypeId);
        request.set_token_name(tokenName);
        request.set_category(category);
        request.set_max_supply(maxSupply);
        request.set_issue_time_span(issueTimeSpan);
        request.set_burnable(burnable);
        request.set_transferable(transferable);
        request.set_sellable(sellable);
        *request.mutable_agreement_ids() = { agreementIds.begin(), agreementIds.end() };
        *request.mutable_metadata() = metadata.ToProto();
        return request;
    }

    template<typename TResponse>
    static IVIResultItemTypeStateChange::PayloadT ParseItemTypeStateUpdate(const TResponse& response)
    {
        return
        {
            response.game_item_type_id(),
            response.tracking_id(), 
            ECast(response.item_type_state())
        };
    }

    IVIResultItemTypeStateChange IVIItemTypeClient::CreateItemType(
        const string& gameItemTypeId,
        const string& tokenName,
        const string& category,
        int32_t maxSupply,
        int32_t issueTimeSpan,
        bool burnable,
        bool transferable,
        bool sellable,
        const UUIDList& agreementIds,
        const IVIMetadata& metadata)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("CreateItemType request: ", gameItemTypeId);

        using Response = proto::api::itemtype::CreateItemAsyncResponse;
        return CallUnary<IVIResultItemTypeStateChange, Response>(
            MakeCreateItemTypeRequest(gameItemTypeId, tokenName, category, maxSupply, issueTimeSpan, burnable, transferable, sellable, agreementIds, metadata),
            &ServiceT::Stub::CreateItemType,
            &ParseItemTypeStateUpdate<Response>);
    }

    void IVIItemTypeClientAsync::CreateItemType(
        const string& gameItemTypeId,
        const string& tokenName,
        const string& category,
        int32_t maxSupply,
        int32_t issueTimeSpan,
        bool burnable,
        bool transferable,
        bool sellable,
        const UUIDList& agreementIds,
        const IVIMetadata& metadata,
        const function<void(const IVIResultItemTypeStateChange&)>& callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("CreateItemType (async) request: ", gameItemTypeId);

        using Response = proto::api::itemtype::CreateItemAsyncResponse;
        CallUnaryAsync<IVIResultItemTypeStateChange, Response>(
            MakeCreateItemTypeRequest(gameItemTypeId, tokenName, category, maxSupply, issueTimeSpan, burnable, transferable, sellable, agreementIds, metadata),
            &ServiceT::Stub::AsyncCreateItemType,
            &ParseItemTypeStateUpdate<Response>,
            callback);
    }

    static proto::api::itemtype::FreezeItemTypeRequest MakeFreezeItemTypeRequest(const string& gameItemTypeId)
    {
        proto::api::itemtype::FreezeItemTypeRequest request;
        request.set_game_item_type_id(gameItemTypeId);
        return request;
    }

    struct FreezeItemTypeAsyncResponseParser
    {
        string gameInventoryId;
        IVIItemTypeStateChange operator()(const proto::api::itemtype::FreezeItemTypeAsyncResponse& response) const
        {
            return
            {
                gameInventoryId,
                response.tracking_id(),
                ECast(response.item_type_state())
            };
        }
    };

    IVIResultItemTypeStateChange IVIItemTypeClient::FreezeItemType(
        const string& gameItemTypeId)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("FreezeItemType request: ", gameItemTypeId);

        using Response = proto::api::itemtype::FreezeItemTypeAsyncResponse;
        return CallUnary<IVIResultItemTypeStateChange, Response>(
            MakeFreezeItemTypeRequest(gameItemTypeId),
            &ServiceT::Stub::FreezeItemType,
            FreezeItemTypeAsyncResponseParser{ gameItemTypeId });
    }


    void IVIItemTypeClientAsync::FreezeItemType(
        const string& gameItemTypeId, 
        const function<void(const IVIResultItemTypeStateChange&)>& callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("FreezeItemType (async) request: ", gameItemTypeId);

        using Response = proto::api::itemtype::FreezeItemTypeAsyncResponse;
        CallUnaryAsync<IVIResultItemTypeStateChange, Response>(
            MakeFreezeItemTypeRequest(gameItemTypeId),
            &ServiceT::Stub::AsyncFreezeItemType,
            FreezeItemTypeAsyncResponseParser{ gameItemTypeId },
            callback);
    }

    static proto::api::itemtype::UpdateItemTypeMetadataPayload MakeUpdateItemTypeMetadataPayload(
        const string& gameItemTypeId, 
        const IVIMetadata& metadata)
    {
        proto::api::itemtype::UpdateItemTypeMetadataPayload request;
        request.set_game_item_type_id(gameItemTypeId);
        *request.mutable_metadata() = metadata.ToProto();
        return request;
    }

    IVIResult IVIItemTypeClient::UpdateItemTypeMetadata(
        const string& gameItemTypeId, 
        const IVIMetadata& metadata)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("UpdateItemTypeMetadata request: ", gameItemTypeId);

        using Response = google::protobuf::Empty;
        return CallUnary<IVIResult, Response>(
            MakeUpdateItemTypeMetadataPayload(gameItemTypeId, metadata),
            &ServiceT::Stub::UpdateItemTypeMetadata,
            nullptr);
    }

    void IVIItemTypeClientAsync::UpdateItemTypeMetadata(
        const string& gameItemTypeId,
        const IVIMetadata& metadata,
        const function<void(const IVIResult&)>& callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("UpdateItemTypeMetadata (async) request: ", gameItemTypeId);

        using Response = google::protobuf::Empty;
        CallUnaryAsync<IVIResult, Response>(
            MakeUpdateItemTypeMetadataPayload(gameItemTypeId, metadata),
            &ServiceT::Stub::AsyncUpdateItemTypeMetadata,
            nullptr,
            callback);
    }

    //////////////////////////////////////////////////////////////////////////
    // Player request clients
    //////////////////////////////////////////////////////////////////////////

    template IVIClientT<IVIPlayerClient::ServiceT>::IVIClientT(
        const IVIConfigurationPtr& configuration,
        const IVIConnectionPtr& conn);
    IVIPlayerClient::~IVIPlayerClient() {}
    IVIPlayerClientAsync::~IVIPlayerClientAsync() {}

    static proto::api::player::LinkPlayerRequest MakeLinkPlayerRequest(
        const string& playerId,
        const string& email,
        const string& displayName,
        const string& requestIp)
    {
        proto::api::player::LinkPlayerRequest request;
        request.set_player_id(playerId);
        request.set_email(email);
        request.set_display_name(displayName);
        request.set_request_ip(requestIp);
        return request;
    }

    struct LinkPlayerAsyncResponseParser
    {
        string playerId;
        IVIResultPlayerStateChange::PayloadT operator()(const proto::api::player::LinkPlayerAsyncResponse& response) const
        {
            return
            {
                playerId,
                response.tracking_id(),
                ECast(response.player_state())
            };
        }
    };

    IVIResultPlayerStateChange IVIPlayerClient::LinkPlayer(
        const string& playerId,
        const string& email,
        const string& displayName,
        const string& requestIp)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("LinkPlayer request: ", playerId);

        using Response = proto::api::player::LinkPlayerAsyncResponse;
        return CallUnary<IVIResultPlayerStateChange, Response>(
            MakeLinkPlayerRequest(playerId, email, displayName, requestIp),
            &ServiceT::Stub::LinkPlayer,
            LinkPlayerAsyncResponseParser{ playerId });
    }

    void IVIPlayerClientAsync::LinkPlayer(
        const string& playerId,
        const string& email,
        const string& displayName,
        const string& requestIp,
        const function<void(const IVIResultPlayerStateChange&)>& callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("LinkPlayer (async) request: ", playerId);

        using Response = proto::api::player::LinkPlayerAsyncResponse;
        CallUnaryAsync<IVIResultPlayerStateChange, Response>(
            MakeLinkPlayerRequest(playerId, email, displayName, requestIp),
            &ServiceT::Stub::AsyncLinkPlayer,
            LinkPlayerAsyncResponseParser{ playerId },
            callback);
    }

    static proto::api::player::GetPlayerRequest MakeGetPlayerRequest(const string& playerId)
    {
        proto::api::player::GetPlayerRequest request;
        request.set_player_id(playerId);
        return request;
    }

    IVIResultPlayer IVIPlayerClient::GetPlayer(const string& playerId)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("GetPlayer request: ", playerId);

        using Response = proto::api::player::IVIPlayer;
        return CallUnary<IVIResultPlayer, Response>(
            MakeGetPlayerRequest(playerId),
            &ServiceT::Stub::GetPlayer,
            &IVIPlayer::FromProto);
    }

    void IVIPlayerClientAsync::GetPlayer(
        const string& playerId,
        const function<void(const IVIResultPlayer&)>& callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("GetPlayer (async) request: ", playerId);

        using Response = proto::api::player::IVIPlayer;
        CallUnaryAsync<IVIResultPlayer, Response>(
            MakeGetPlayerRequest(playerId),
            &ServiceT::Stub::AsyncGetPlayer,
            &IVIPlayer::FromProto,
            callback);
    }

    static proto::api::player::GetPlayersRequest MakeGetPlayersRequest(
        time_t createdTimestamp,
        int32_t pageSize,
        SortOrder sortOrder)
    {
        proto::api::player::GetPlayersRequest request;
        request.set_created_timestamp(createdTimestamp);
        request.set_page_size(pageSize);
        request.set_sort_order(ECast(sortOrder));
        return request;
    }

    static IVIResultPlayerList::PayloadT ParseIVIPlayers(const proto::api::player::IVIPlayers& response)
    {
        IVIPlayerList responseList;
        transform(response.ivi_players().begin(), response.ivi_players().end(), back_inserter(responseList), &IVIPlayer::FromProto);
        return responseList;
    }

    IVIResultPlayerList IVIPlayerClient::GetPlayers(
        time_t createdTimestamp,
        int32_t pageSize,
        SortOrder sortOrder)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("GetPlayers request: ", pageSize);

        using Response = proto::api::player::IVIPlayers;
        return CallUnary<IVIResultPlayerList, Response>(
            MakeGetPlayersRequest(createdTimestamp, pageSize, sortOrder),
            &ServiceT::Stub::GetPlayers,
            &ParseIVIPlayers);
    }

    void IVIPlayerClientAsync::GetPlayers(
        time_t createdTimestamp,
        int32_t pageSize,
        SortOrder sortOrder,
        const function<void(const IVIResultPlayerList&)>& callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("GetPlayers (async) request: ", pageSize);

        using Response = proto::api::player::IVIPlayers;
        CallUnaryAsync<IVIResultPlayerList, Response>(
            MakeGetPlayersRequest(createdTimestamp, pageSize, sortOrder),
            &ServiceT::Stub::AsyncGetPlayers,
            &ParseIVIPlayers,
            callback);
    }

    //////////////////////////////////////////////////////////////////////////
    // Order request clients
    //////////////////////////////////////////////////////////////////////////

    template IVIClientT<IVIOrderClient::ServiceT>::IVIClientT(
        const IVIConfigurationPtr& configuration,
        const IVIConnectionPtr& conn);
    IVIOrderClient::~IVIOrderClient() {}
    IVIOrderClientAsync::~IVIOrderClientAsync() {}

    static proto::api::order::GetOrderRequest MakeGetOrderRequest(const string& orderId)
    {
        proto::api::order::GetOrderRequest request;
        request.set_order_id(orderId);
        return request;
    }

    IVIResultOrder IVIOrderClient::GetOrder(
        const string& orderId)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("GetOrder request: ", orderId);

        using Response = proto::api::order::Order;
        return CallUnary<IVIResultOrder, Response>(
            MakeGetOrderRequest(orderId),
            &ServiceT::Stub::GetOrder,
            &IVIOrder::FromProto);
    }

    void IVIOrderClientAsync::GetOrder(
        const string& orderId,
        const function<void(const IVIResultOrder&)> callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("GetOrder (async) request: ", orderId);

        using Response = proto::api::order::Order;
        CallUnaryAsync<IVIResultOrder, Response>(
            MakeGetOrderRequest(orderId),
            &ServiceT::Stub::AsyncGetOrder,
            &IVIOrder::FromProto, 
            callback);
    }

    static proto::api::order::CreateOrderRequest MakeCreateOrderRequest(
        const string& storeId,
        const string& buyerPlayerId,
        const BigDecimal& subTotal,
        const IVIOrderAddress& address,
        PaymentProviderId paymentProviderId,
        const IVIPurchasedItemsList& purchasedItems,
        const string& metadata,
        const string& requestIp)
    {
        proto::api::order::CreateOrderRequest request;
        request.set_store_id(storeId);
        request.set_buyer_player_id(buyerPlayerId);
        request.set_sub_total(subTotal);
        *request.mutable_address() = address.ToProto();
        request.set_payment_provider_id(ECast(paymentProviderId));
        if(metadata.size() > 0)
            *request.mutable_metadata() = JsonStringToGoogleStruct(metadata);
        request.set_request_ip(requestIp);
        transform(purchasedItems.begin(), purchasedItems.end(),
            google::protobuf::RepeatedPtrFieldBackInserter(request.mutable_purchased_items()->mutable_purchased_items()),
            [](const IVIPurchasedItems& item) { return item.ToProto(); });
        return request;
    }

    IVIResultOrder IVIOrderClient::CreatePrimaryOrder(
        const string& storeId, 
        const string& buyerPlayerId, 
        const BigDecimal& subTotal, 
        const IVIOrderAddress& address, 
        PaymentProviderId paymentProviderId, 
        const IVIPurchasedItemsList& purchasedItems, 
        const string& metadata, 
        const string& requestIp)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("CreatePrimaryOrder request: ", buyerPlayerId);

        using Response = proto::api::order::Order;
        return CallUnary<IVIResultOrder, Response>(
            MakeCreateOrderRequest(storeId, buyerPlayerId, subTotal, address, paymentProviderId, purchasedItems, metadata, requestIp),
            &ServiceT::Stub::CreateOrder,
            &IVIOrder::FromProto);
    }

    void IVIOrderClientAsync::CreatePrimaryOrder(
        const string& storeId,
        const string& buyerPlayerId,
        const BigDecimal& subTotal,
        const IVIOrderAddress& address,
        PaymentProviderId paymentProviderId,
        const IVIPurchasedItemsList& purchasedItems,
        const string& metadata,
        const string& requestIp,
        const function<void(const IVIResultOrder&)> callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("CreatePrimaryOrder (async) request: ", buyerPlayerId);

        using Response = proto::api::order::Order;
        CallUnaryAsync<IVIResultOrder, Response>(
            MakeCreateOrderRequest(storeId, buyerPlayerId, subTotal, address, paymentProviderId, purchasedItems, metadata, requestIp),
            &ServiceT::Stub::AsyncCreateOrder,
            &IVIOrder::FromProto,
            callback);
    }

    proto::api::order::FinalizeOrderRequest MakeFinalizeOrderRequest(
        const string& orderId,
        const string& fraudSessionId,
        proto::api::order::PaymentRequestProto paymentRequest)
    {
        proto::api::order::FinalizeOrderRequest request;
        request.set_order_id(orderId);
        request.set_fraud_session_id(fraudSessionId);
        *request.mutable_payment_request_data() = move(paymentRequest);
        return request;
    }

    proto::api::order::PaymentRequestProto MakePaymentRequestProtoBraintree(
        const string& clientToken,
        const string& paymentNonce)
    {
        proto::api::order::PaymentRequestProto request;
        request.mutable_braintree()->set_braintree_client_token(clientToken);
        request.mutable_braintree()->set_braintree_payment_nonce(paymentNonce);
        return request;
    }

    proto::api::order::PaymentRequestProto MakePaymentRequestProtoBitpay(
        const string& invoiceId)
    {
        proto::api::order::PaymentRequestProto request;
        request.mutable_bitpay()->set_invoice_id(invoiceId);
        return request;
    }

    IVIResultFinalizeOrderResponse IVIOrderClient::FinalizeBraintreeOrder(
        const string& orderId,
        const string& clientToken,
        const string& paymentNonce,
        const string& fraudSessionId)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("FinalizeBraintreeOrder request: ", orderId);

        return FinalizeOrder(
            orderId, fraudSessionId, MakePaymentRequestProtoBraintree(clientToken, paymentNonce));
    }

    IVIResultFinalizeOrderResponse IVIOrderClient::FinalizeBitpayOrder(
        const string& orderId,
        const string& invoiceId,
        const string& fraudSessionId)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("FinalizeBitpayOrder request: ", orderId);

        return FinalizeOrder(
            orderId, fraudSessionId, MakePaymentRequestProtoBitpay(invoiceId));
    }

    IVIResultFinalizeOrderResponse IVIOrderClient::FinalizeOrder(
        const string& orderId,
        const string& fraudSessionId,
        proto::api::order::PaymentRequestProto paymentData)
    {
        using Response = proto::api::order::FinalizeOrderAsyncResponse;
        return CallUnary<IVIResultFinalizeOrderResponse, Response>(
            MakeFinalizeOrderRequest(orderId, fraudSessionId, move(paymentData)),
            &ServiceT::Stub::FinalizeOrder,
            &IVIFinalizeOrderResponse::FromProto);
    }

    void IVIOrderClientAsync::FinalizeBraintreeOrder(
        const string& orderId,
        const string& clientToken,
        const string& paymentNonce,
        const string& fraudSessionId,
        const function<void(const IVIResultFinalizeOrderResponse&)> callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("FinalizeBraintreeOrder (async) request: ", orderId);

        FinalizeOrder(
            orderId, fraudSessionId, MakePaymentRequestProtoBraintree(clientToken, paymentNonce), callback);
    }

    void IVIOrderClientAsync::FinalizeBitpayOrder(
        const string& orderId,
        const string& invoiceId,
        const string& fraudSessionId,
        const function<void(const IVIResultFinalizeOrderResponse&)> callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("FinalizeBitpayOrder request: ", orderId);

        FinalizeOrder(
            orderId, fraudSessionId, MakePaymentRequestProtoBitpay(invoiceId), callback);
    }

    void IVIOrderClientAsync::FinalizeOrder(
        const string& orderId,
        const string& fraudSessionId,
        proto::api::order::PaymentRequestProto paymentData,
        const function<void(const IVIResultFinalizeOrderResponse&)> callback)
    {
        using Response = proto::api::order::FinalizeOrderAsyncResponse;
        CallUnaryAsync<IVIResultFinalizeOrderResponse, Response>(
            MakeFinalizeOrderRequest(orderId, fraudSessionId, move(paymentData)),
            &ServiceT::Stub::AsyncFinalizeOrder,
            &IVIFinalizeOrderResponse::FromProto,
            callback);
    }

    //////////////////////////////////////////////////////////////////////////
    // Payment request clients
    //////////////////////////////////////////////////////////////////////////

    template IVIClientT<IVIPaymentClient::ServiceT>::IVIClientT(
        const IVIConfigurationPtr& configuration,
        const IVIConnectionPtr& conn);
    IVIPaymentClient::~IVIPaymentClient() {}
    IVIPaymentClientAsync::~IVIPaymentClientAsync() {}

    static proto::api::payment::CreateTokenRequest MakeCreateTokenRequest(
        PaymentProviderId id,
        const string& playerId)
    {
        IVI_CHECK(id == PaymentProviderId::BRAINTREE);
        proto::api::payment::CreateTokenRequest request;
        request.mutable_braintree()->set_player_id(playerId);
        return request;
    }

    IVIResultToken IVIPaymentClient::GetToken(
        PaymentProviderId id,
        const string& playerId)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("GetToken request: ", playerId);

        using Response = proto::api::payment::Token;
        return CallUnary<IVIResultToken, Response>(
            MakeCreateTokenRequest(id, playerId),
            &ServiceT::Stub::GenerateClientToken,
            &IVIToken::FromProto);
    }

    void IVIPaymentClientAsync::GetToken(
        PaymentProviderId id,
        const string& playerId,
        const function<void(const IVIResultToken&)>& callback)
    {
        IVI_LOG_FUNC();
        IVI_LOG_VERBOSE("GetToken (async) request: ", playerId);

        using Response = proto::api::payment::Token;
        CallUnaryAsync<IVIResultToken, Response>(
            MakeCreateTokenRequest(id, playerId),
            &ServiceT::Stub::AsyncGenerateClientToken,
            &IVIToken::FromProto,
            callback);
    }

    //////////////////////////////////////////////////////////////////////////
    // Item stream client
    //////////////////////////////////////////////////////////////////////////

    template IVIItemStreamClientTraits::CallbackType IVIStreamClientT<IVIItemStreamClientTraits>::GetCallback() const;
    template void IVIStreamClientT<IVIItemStreamClientTraits>::Finish();
    template bool IVIStreamClientT<IVIItemStreamClientTraits>::IsFinished();

    IVIItemStreamClient::IVIItemStreamClient(
        const IVIConfigurationPtr& configuration,
        const IVIConnectionPtr& conn,
        const OnItemUpdated& onItemUpdated)
        : IVIStreamClientT(
            configuration, 
            conn, 
            onItemUpdated,
            &ServiceT::Stub::AsyncItemStatusStream, // subscribe
            [this]()    // sendConfirm
            {
                const rpc::streams::item::ItemStatusUpdate& response(CurrentMessage());
                Confirm(
                    response.game_inventory_id(),
                    response.tracking_id(),
                    ECast(response.item_state()));
            })
    {
        IVI_LOG_FUNC_TRIVIAL();
    }

    void IVIItemStreamClient::Confirm(
        const string& gameInventoryId, 
        const string& trackingId, 
        ItemState itemState)
    {
        IVI_LOG_FUNC();
        IVIStreamClientT::Confirm(
            [&]()    // requestCreator
            {
                rpc::streams::item::ItemStatusConfirmRequest request;
                request.set_game_inventory_id(gameInventoryId);
                request.set_tracking_id(trackingId);
                request.set_item_state(ECast(itemState));
                return request;
            },
            &ServiceT::Stub::AsyncItemStatusConfirmation  // confirmRequestFunc
        );
    }

    //////////////////////////////////////////////////////////////////////////
    // ItemType stream client
    //////////////////////////////////////////////////////////////////////////

    template IVIItemTypeStreamClientTraits::CallbackType IVIStreamClientT<IVIItemTypeStreamClientTraits>::GetCallback() const;
    template void IVIStreamClientT<IVIItemTypeStreamClientTraits>::Finish();
    template bool IVIStreamClientT<IVIItemTypeStreamClientTraits>::IsFinished();

    IVIItemTypeStreamClient::IVIItemTypeStreamClient(
        const IVIConfigurationPtr& configuration,
        const IVIConnectionPtr& conn,
        const OnItemTypeUpdated& onItemTypeUpdated)
        : IVIStreamClientT(
            configuration,
            conn,
            onItemTypeUpdated,
            &ServiceT::Stub::AsyncItemTypeStatusStream, // subscribe
            [this]()    // sendConfirm
            {
                const rpc::streams::itemtype::ItemTypeStatusUpdate& response(CurrentMessage());
                Confirm(
                    response.game_item_type_id(),
                    response.tracking_id(),
                    ECast(response.item_type_state()));
            })
    {
        IVI_LOG_FUNC_TRIVIAL();
    }

    void IVIItemTypeStreamClient::Confirm(
        const string& gameItemTypeId,
        const string& trackingId,
        ItemTypeState itemTypeState)
    {
        IVI_LOG_FUNC();
        IVIStreamClientT::Confirm(
            [&]()    // requestCreator
            {
                rpc::streams::itemtype::ItemTypeStatusConfirmRequest request;
                request.set_game_item_type_id(gameItemTypeId);
                request.set_tracking_id(trackingId);
                request.set_item_type_state(ECast(itemTypeState));
                return request;
            },
            &ServiceT::Stub::AsyncItemTypeStatusConfirmation  // confirmRequestFunc
        );
    }

    //////////////////////////////////////////////////////////////////////////
    // Order stream client
    //////////////////////////////////////////////////////////////////////////

    template IVIOrderStreamClientTraits::CallbackType IVIStreamClientT<IVIOrderStreamClientTraits>::GetCallback() const;
    template void IVIStreamClientT<IVIOrderStreamClientTraits>::Finish();
    template bool IVIStreamClientT<IVIOrderStreamClientTraits>::IsFinished();

    IVIOrderStreamClient::IVIOrderStreamClient(
        const IVIConfigurationPtr& configuration,
        const IVIConnectionPtr& conn,
        const OnOrderUpdated& onOrderUpdated)
        : IVIStreamClientT(
            configuration,
            conn,
            onOrderUpdated,
            &ServiceT::Stub::AsyncOrderStatusStream, // subscribe
            [this]()    // sendConfirm
            {
                const rpc::streams::order::OrderStatusUpdate& response(CurrentMessage());
                Confirm(
                    response.order_id(),
                    ECast(response.order_state()));
            })
    {
        IVI_LOG_FUNC_TRIVIAL();
    }

    void IVIOrderStreamClient::Confirm(
        const string& orderId,
        OrderState orderState)
    {
        IVI_LOG_FUNC();
        IVIStreamClientT::Confirm(
            [&]()    // requestCreator
            {
                rpc::streams::order::OrderStatusConfirmRequest request;
                request.set_order_id(orderId);
                request.set_order_state(ECast(orderState));
                return request;
            },
            &ServiceT::Stub::AsyncOrderStatusConfirmation  // confirmRequestFunc
        );
    }

    //////////////////////////////////////////////////////////////////////////
    // Player stream client
    //////////////////////////////////////////////////////////////////////////

    template IVIPlayerStreamClientTraits::CallbackType IVIStreamClientT<IVIPlayerStreamClientTraits>::GetCallback() const;
    template void IVIStreamClientT<IVIPlayerStreamClientTraits>::Finish();
    template bool IVIStreamClientT<IVIPlayerStreamClientTraits>::IsFinished();

    IVIPlayerStreamClient::IVIPlayerStreamClient(
        const IVIConfigurationPtr& configuration,
        const IVIConnectionPtr& conn,
        const OnPlayerUpdated& onOrderUpdated)
        : IVIStreamClientT(
            configuration,
            conn,
            onOrderUpdated,
            &ServiceT::Stub::AsyncPlayerStatusStream, // subscribe
            [this]()    // sendConfirm
            {
                const rpc::streams::player::PlayerStatusUpdate& response(CurrentMessage());
                Confirm(
                    response.player_id(),
                    response.tracking_id(),
                    ECast(response.player_state()));
            })
    {
        IVI_LOG_FUNC_TRIVIAL();
    }

    void IVIPlayerStreamClient::Confirm(
                const string& playerId,
                const string& trackingId,
                PlayerState playerState)
    {
        IVI_LOG_FUNC();
        IVIStreamClientT::Confirm(
            [&]()    // requestCreator
            {
                rpc::streams::player::PlayerStatusConfirmRequest request;
                request.set_player_id(playerId);
                request.set_tracking_id(trackingId);
                request.set_player_state(ECast(playerState));
                return request;
            },
            &ServiceT::Stub::AsyncPlayerStatusConfirmation  // confirmRequestFunc
        );
    }
}
