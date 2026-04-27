-module(udp_server).
-behaviour(gen_server).

-export([start_link/2]).
-export([init/1, handle_call/3, handle_cast/2, handle_info/2, terminate/2, code_change/3]).

-define(SERVER, ?MODULE).

-record(state, {
    socket :: inet:socket(),
    port :: inet:port_number()
}).

%% ------------------------------------------------------------------
%% API
%% ------------------------------------------------------------------

start_link(Port, Workers) ->
    gen_server:start_link({local, ?SERVER}, ?MODULE, [Port, Workers], []).

%% ------------------------------------------------------------------
%% gen_server callbacks
%% ------------------------------------------------------------------

init([Port, _Workers]) ->
    process_flag(trap_exit, true),
    case gen_udp:open(Port, [
        binary,
        {active, 100},
        {recbuf, 65536},
        {sndbuf, 65536},
        {read_packets, 100}
    ]) of
        {ok, Socket} ->
            {ok, ActualPort} = inet:port(Socket),
            error_logger:info_msg("UDP server listening on port ~p~n", [ActualPort]),
            {ok, #state{socket = Socket, port = ActualPort}};
        {error, Reason} ->
            {stop, Reason}
    end.

handle_call(get_port, _From, #state{port = Port} = State) ->
    {reply, {ok, Port}, State};
handle_call(_Request, _From, State) ->
    {reply, {error, unknown_call}, State}.

handle_cast(_Msg, State) ->
    {noreply, State}.

handle_info({udp, Socket, IP, InPortNo, Packet}, #state{socket = Socket} = State) ->
    case protocol:decode(Packet) of
        {ok, #{command := Cmd, seq := Seq, payload := Payload}} ->
            spawn_link(fun() -> handle_packet(Cmd, Seq, Payload, Socket, IP, InPortNo) end);
        {error, Reason} ->
            error_logger:warning_msg("Bad packet from ~p:~p: ~p~n", [IP, InPortNo, Reason])
    end,
    {noreply, State};

handle_info({udp_error, Socket, Reason}, #state{socket = Socket} = State) ->
    error_logger:error_msg("UDP socket error: ~p~n", [Reason]),
    {stop, {udp_error, Reason}, State};

handle_info({udp_passive, Socket}, #state{socket = Socket} = State) ->
    inet:setopts(Socket, [{active, 100}]),
    {noreply, State};

handle_info(_Info, State) ->
    {noreply, State}.

terminate(_Reason, #state{socket = Socket}) ->
    gen_udp:close(Socket),
    ok.

code_change(_OldVsn, State, _Extra) ->
    {ok, State}.

%% ------------------------------------------------------------------
%% Internal
%% ------------------------------------------------------------------

handle_packet(check, Seq, Payload, Socket, IP, Port) ->
    %% Client asks: "Do you have updates for me?"
    %% Payload: JSON binary #{"current_version" => "1.2.0", "platform" => "windows"}
    Reply = update_handler:handle_check(Payload),
    send_reply(Socket, IP, Port, announce, Seq, Reply);

handle_packet(pull, Seq, Payload, Socket, IP, Port) ->
    %% Client requests actual update payload
    Reply = update_handler:handle_pull(Payload),
    send_reply(Socket, IP, Port, push, Seq, Reply);

handle_packet(ack, Seq, Payload, _Socket, _IP, _Port) ->
    error_logger:info_msg("ACK received seq=~p payload=~p~n", [Seq, Payload]),
    ok;

handle_packet(Cmd, Seq, _Payload, Socket, IP, Port) ->
    %% Server-initiated commands (announce, push) should not arrive unsolicited
    error_logger:warning_msg("Unexpected command ~p from ~p:~p seq=~p~n", [Cmd, IP, Port, Seq]),
    send_reply(Socket, IP, Port, ack, Seq, <<>>).

send_reply(Socket, IP, Port, Cmd, Seq, Payload) ->
    case protocol:encode(Cmd, Seq, Payload) of
        {ok, Packet} ->
            gen_udp:send(Socket, IP, Port, Packet);
        {error, Reason} ->
            error_logger:error_msg("Failed to encode reply: ~p~n", [Reason]),
            {error, Reason}
    end.
