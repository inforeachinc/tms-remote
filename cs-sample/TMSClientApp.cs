using System;
using System.IO;
using System.Collections.Generic;
using System.Threading;
using System.Threading.Tasks;
using Grpc.Core;


namespace TMSClientApp
{
    class TMSClientApp
    {
        private const string SERVER = "localhost";
        private const int GRPCPORT = 8083;
        private const string USER = "demo";
        private const string PASSWORD = "";

        private static readonly string PORTFOLIO = "grpcSample - " + DateTime.Now.ToString("yyyyMMdd HH:mm:ss");

        static async Task Main(string[] args)
        {
            var channelCredentials = new SslCredentials(File.ReadAllText("cert.pem"));
            var channel = new Channel(SERVER, GRPCPORT, channelCredentials);

            var client = new TMSRemote.TMSRemoteClient(channel);
            client.login(new LoginRequest { User = USER, Password = PASSWORD });

            // subscribe for orders
            
            var ordersStreamingCall = new StreamingCall<SubscribeForOrdersRequest, OrderEvent>(client, client.subscribeForOrders(), OnOrderEvent);
            await ordersStreamingCall.SendAsync(new SubscribeForOrdersRequest {
                Filter = "Portfolio = '" + PORTFOLIO + "'",
                Field = { "TgtID", "Instrument", "Leaves", "OrdPx", "OrdQty" }
            });

            try
            {
                // create market portfolio

                try
                {
                    var status = client.createMarketPortfolio(new CreateMarketPortfolioRequest {
                        Name = PORTFOLIO,
                        Type = PortfolioType.Pure
                    });
                }
                catch (RpcException ex)
                {
                    var errorCode = ex.Trailers.GetValue("errorcode");
                    if (!"CannotCreatePortfolio".Equals(errorCode))
                        throw ex;
                    Console.WriteLine("Portfolio already exists");
                }

                // add market targets

                var targets = new List<Fields>();
                foreach (var instrument in new string[] { "IBM", "GOOG" })
                {
                    var target = new Fields();    
                    target.StringFields["Instrument"] = instrument;
                    target.NumericFields["Side"] = (double)Side.Buy;
                    target.NumericFields["TgtQty"] = 500;
                    target.NumericFields["TgtOrdType"] = (double)OrdType.Limit;
                    target.StringFields["SetPxTo"] = "BidPx:AskPx";
                    targets.Add(target);
                }

                var targetIds = client.addMarketTargets(new AddMarketTargetsRequest { 
                    Portfolio = PORTFOLIO,
                    Fields = { targets }
                });

                // modify market targets

                var fields = new Fields();
                fields.StringFields["TrnDestination"] = "Simulator1";
                fields.NumericFields["WaveSizeType"] = (double)WaveSizeType.PctTgtQty;
                fields.NumericFields["WaveSize"] = 10;

                client.modifyMarketTargets(new ModifyTargetsRequest {
                    TargetId = { targetIds.TargetId },
                    Fields = { fields }
                });

                // send target orders

                client.sendOrders(new SendOrdersRequest {
                    TargetId = { targetIds.TargetId },
                });

                // send target orders with params

                var messages = new List<FIXFields>();
                for (int i = 0; i < targetIds.TargetId.Count; i++)
                {
                    var fixFields = new FIXFields();
                    fixFields.NumericFields[(int)FIXTag.OrderQty] = 200;
                    messages.Add(fixFields);
                }

                client.sendOrders(new SendOrdersRequest {
                    TargetId = { targetIds.TargetId },
                    Message = { messages }
                });

                // list targets

                var targetsStreamingCall = new StreamingCall<SubscribeForTargetsRequest, TargetEvent>(client, client.subscribeForMarketTargets(), OnTargetEvent);
                try
                {
                    await targetsStreamingCall.SendAsync(new SubscribeForTargetsRequest {
                        Field = { "TgtID", "Portfolio", "Instrument", "Side", "TgtQty", "FillQty" }
                    });
                    await targetsStreamingCall.GetReaderTask();
                }
                finally
                {
                    await targetsStreamingCall.DisposeAsync();
                }


                await Task.Delay(60_000);
                Console.WriteLine("Done.");
            }
            catch (RpcException ex)
            {
                var exceptionClass = ex.Trailers.GetValue("exceptionclass");
                var errorCode = ex.Trailers.GetValue("errorcode");
                Console.WriteLine("Exception class: " + exceptionClass + ", errorCode: " + errorCode);
                if (int.TryParse(ex.Trailers.GetValue("childexceptionscount"), out int childExceptionsCount))
                {
                    for (int i = 0; i < Math.Min(childExceptionsCount, 10); i++) //only 10 child exceptions have details sent remotely 
                    {
                        Console.WriteLine("  Child exception " + i + ": " + ex.Trailers.Get("childexceptionmessage_" + i).Value);
                    }
                }
                throw ex;
            }
            finally
            {
                await ordersStreamingCall.DisposeAsync();
            }
        }

        public static async Task<bool> OnOrderEvent(TMSRemote.TMSRemoteClient client, OrderEvent response)
        {
            switch (response.EventCase)
            {
                case OrderEvent.EventOneofCase.Added:
                {
                    var order = response.Added;
                    var orderId = order.OrderId;
                    var instrument = order.Fields.StringFields.GetValueOrDefault("Instrument", "-");
                    var ordQty = order.Fields.NumericFields.GetValueOrDefault("OrdQty", 0);
                    var ordPx = order.Fields.NumericFields.GetValueOrDefault("OrdPx", double.NaN);
                    Console.WriteLine("Order added: " + orderId + " " + instrument + " " + ordQty + " " + ordPx);

                    // modify order

                    var fixFields = new FIXFields();
                    fixFields.NumericFields[(int)FIXTag.Price] = Math.Round(ordPx * 1.05, 2);

                    await client.modifyOrdersAsync(new ModifyOrdersRequest {
                        OrderId = { orderId },
                        Message = { fixFields }
                    });

                    break;
                }
                case OrderEvent.EventOneofCase.Updated:
                {
                    var order = response.Updated;
                    var orderId = order.OrderId;
                    var leaves = order.Fields.NumericFields.GetValueOrDefault("Leaves", double.NaN);
                    if (double.IsFinite(leaves) && leaves == 0)
                    {
                        Console.WriteLine("Order closed: " + orderId);
                    }
                    break;
                }
                default:
                    break;
            }
            return true;
        }

        public static Task<bool> OnTargetEvent(TMSRemote.TMSRemoteClient client, TargetEvent response)
        {
            switch (response.EventCase)
            {
                case TargetEvent.EventOneofCase.Added:
                {
                    var target = response.Added;
                    Console.WriteLine("Target " +
                        target.Fields.NumericFields.GetValueOrDefault("TgtID", double.NaN) + ", " +
                        target.Fields.StringFields.GetValueOrDefault("Portfolio", "-") + ", " + 
                        target.Fields.StringFields.GetValueOrDefault("Instrument", "-")  + ", " +
                        target.Fields.NumericFields.GetValueOrDefault("Side", double.NaN)  + ", " +
                        target.Fields.NumericFields.GetValueOrDefault("TgtQty", double.NaN)  + ", " +
                        target.Fields.NumericFields.GetValueOrDefault("FillQty", double.NaN)
                    );
                    break;
                }
                case TargetEvent.EventOneofCase.FeedStatus:
                {
                    if (response.FeedStatus == FeedStatus.InitialStateReceived)
                    {
                        return Task.FromResult(false);
                    }
                    break;
                }
                default:
                    break;
            }
            return Task.FromResult(true);
        }
    }


    class StreamingCall<TRequest, TResponse>
    {
        private readonly TMSRemote.TMSRemoteClient client_;
        private readonly AsyncDuplexStreamingCall<TRequest, TResponse> streamingCall_;
        private readonly Listener listener_;
        private readonly CancellationTokenSource cancellationTokenSource_;
        private readonly Task readerTask_;
        
        public delegate Task<bool> Listener(TMSRemote.TMSRemoteClient client, TResponse response);

        public StreamingCall(TMSRemote.TMSRemoteClient client, AsyncDuplexStreamingCall<TRequest, TResponse> streamingCall, Listener listener)
        {
            client_ = client;
            streamingCall_ = streamingCall;
            listener_ = listener;
            cancellationTokenSource_ = new CancellationTokenSource();
            readerTask_ = ReadAsync(cancellationTokenSource_.Token);
        }

        public async Task SendAsync(TRequest request)
        {
            await streamingCall_.RequestStream.WriteAsync(request);
        }

        public async Task ReadAsync(CancellationToken token)
        {
            while (await streamingCall_.ResponseStream.MoveNext(token))
            {
                if (!await listener_.Invoke(client_, streamingCall_.ResponseStream.Current))
                {
                    return;
                }
            }
        }

        public Task GetReaderTask()
        {
            return readerTask_;
        }

        public async ValueTask DisposeAsync()
        {
            await streamingCall_.RequestStream.CompleteAsync();
            await readerTask_;
            streamingCall_.Dispose();
        }
    }
}
