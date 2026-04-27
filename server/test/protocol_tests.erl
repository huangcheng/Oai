-module(protocol_tests).
-include_lib("eunit/include/eunit.hrl").

encode_decode_test() ->
    Payload = <<"hello">>,
    {ok, Encoded} = protocol:encode(check, 42, Payload),
    ?assertEqual({ok, #{command => check, seq => 42, payload => Payload}},
                 protocol:decode(Encoded)).

empty_payload_test() ->
    {ok, Encoded} = protocol:encode(ack, 0),
    ?assertEqual({ok, #{command => ack, seq => 0, payload => <<>>}},
                 protocol:decode(Encoded)).

truncated_test() ->
    ?assertEqual({error, truncated}, protocol:decode(<<1,2,3>>)).

bad_magic_test() ->
    {ok, Encoded} = protocol:encode(check, 1, <<>>),
    <<_:4/binary, Rest/binary>> = Encoded,
    Bad = <<"BAD!", Rest/binary>>,
    ?assertEqual({error, bad_magic}, protocol:decode(Bad)).

bad_checksum_test() ->
    {ok, Encoded} = protocol:encode(check, 1, <<>>),
    Corrupted = binary:replace(Encoded, <<1>>, <<2>>),
    ?assertEqual({error, bad_checksum}, protocol:decode(Corrupted)).

crc16_known_test() ->
    %% CRC16-CCITT of "123456789" with init 0xFFFF is 0xE5CC
    ?assertEqual(16#E5CC, protocol:crc16(<<"123456789">>)).
