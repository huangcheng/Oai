-module(protocol).
-export([
    encode/2, encode/3,
    decode/1,
    command_name/1,
    crc16/1
]).

%% ------------------------------------------------------------------
%% Datagram Format
%% ------------------------------------------------------------------
%%
%%  +------+---------+---------+------+-------------+---------+
%%  | Magic| Version | Command | Seq  | PayloadLen  | Payload |
%%  | 4 B  | 1 B     | 1 B     | 2 B  | 2 B         | N B     |
%%  +------+---------+---------+------+-------------+---------+
%%  | Checksum (CRC16-CCITT)                                  |
%%  | 2 B                                                     |
%%  +---------------------------------------------------------+
%%
%%  Magic       = <<"HCH",1>>      (4 bytes)
%%  Version     = 1                (1 byte)
%%  Command     = see command_name/1
%%  Seq         = uint16 big-endian
%%  PayloadLen  = uint16 big-endian
%%  Payload     = PayloadLen bytes (max 1384 to stay under typical MTU)
%%  Checksum    = CRC16-CCITT over all preceding bytes
%%
%% ------------------------------------------------------------------

-define(MAGIC, <<"HCH", 1>>).
-define(VERSION, 1).
-define(MAX_PAYLOAD, 1384).

-type command() :: check | announce | pull | push | ack.
-type packet() :: #{
    command := command(),
    seq := non_neg_integer(),
    payload := binary()
}.

-export_type([command/0, packet/0]).

%% ------------------------------------------------------------------
%% Encode
%% ------------------------------------------------------------------

-spec encode(command(), non_neg_integer()) -> {ok, binary()} | {error, term()}.
encode(Cmd, Seq) ->
    encode(Cmd, Seq, <<>>).

-spec encode(command(), non_neg_integer(), binary()) -> {ok, binary()} | {error, term()}.
encode(Cmd, Seq, Payload) when byte_size(Payload) =< ?MAX_PAYLOAD ->
    CmdInt = command_to_int(Cmd),
    PayloadLen = byte_size(Payload),
    Body = <<?MAGIC/binary, ?VERSION:8, CmdInt:8, Seq:16/big, PayloadLen:16/big, Payload/binary>>,
    Crc = crc16(Body),
    {ok, <<Body/binary, Crc:16/big>>};
encode(_, _, Payload) ->
    {error, {payload_too_large, byte_size(Payload)}}.

%% ------------------------------------------------------------------
%% Decode
%% ------------------------------------------------------------------

-spec decode(binary()) -> {ok, packet()} | {error, term()}.
decode(<<>>) ->
    {error, empty};
decode(Packet) when byte_size(Packet) < 12 ->
    {error, truncated};
decode(<<Magic:4/binary, Version:8, CmdInt:8, Seq:16/big, Len:16/big, Rest/binary>> = Packet) ->
    case validate_magic(Magic) of
        ok ->
            case validate_version(Version) of
                ok ->
                    case byte_size(Rest) >= Len + 2 of
                        true ->
                            <<Payload:Len/binary, GivenCrc:16/big>> = binary:part(Rest, 0, Len + 2),
                            BodyLen = byte_size(Packet) - 2,
                            Body = binary:part(Packet, 0, BodyLen),
                            ExpectedCrc = crc16(Body),
                            if
                                GivenCrc =:= ExpectedCrc ->
                                    try
                                        Cmd = int_to_command(CmdInt),
                                        {ok, #{command => Cmd, seq => Seq, payload => Payload}}
                                    catch
                                        throw:unknown_command ->
                                            {error, {unknown_command, CmdInt}}
                                    end;
                                true ->
                                    {error, bad_checksum}
                            end;
                        false ->
                            {error, truncated}
                    end;
                Error ->
                    Error
            end;
        Error ->
            Error
    end.

validate_magic(?MAGIC) -> ok;
validate_magic(_) -> {error, bad_magic}.

validate_version(?VERSION) -> ok;
validate_version(V) -> {error, {bad_version, V}}.

%% ------------------------------------------------------------------
%% Command helpers
%% ------------------------------------------------------------------

-spec command_name(command()) -> string().
command_name(check)    -> "CHECK";
command_name(announce) -> "ANNOUNCE";
command_name(pull)     -> "PULL";
command_name(push)     -> "PUSH";
command_name(ack)      -> "ACK".

-spec command_to_int(command()) -> 1..5.
command_to_int(check)    -> 1;
command_to_int(announce) -> 2;
command_to_int(pull)     -> 3;
command_to_int(push)     -> 4;
command_to_int(ack)      -> 5.

-spec int_to_command(1..5) -> command().
int_to_command(1) -> check;
int_to_command(2) -> announce;
int_to_command(3) -> pull;
int_to_command(4) -> push;
int_to_command(5) -> ack;
int_to_command(_) -> throw(unknown_command).

%% ------------------------------------------------------------------
%% CRC16-CCITT (0xFFFF init, polynomial 0x1021)
%% ------------------------------------------------------------------

-spec crc16(binary()) -> non_neg_integer().
crc16(Data) ->
    crc16(Data, 16#FFFF).

crc16(<<>>, Acc) ->
    Acc;
crc16(<<Byte:8, Rest/binary>>, Acc) ->
    NewAcc = crc16_byte(Byte, Acc),
    crc16(Rest, NewAcc).

crc16_byte(Byte, Acc) ->
    crc16_loop(Acc bxor (Byte bsl 8), 8).

crc16_loop(Acc, 0) ->
    Acc band 16#FFFF;
crc16_loop(Acc, N) ->
    NewAcc = (Acc bsl 1) band 16#FFFF,
    case Acc band 16#8000 of
        0 -> crc16_loop(NewAcc, N - 1);
        _ -> crc16_loop(NewAcc bxor 16#1021, N - 1)
    end.
