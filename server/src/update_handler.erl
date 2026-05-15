-module(update_handler).
-export([handle_check/1, handle_pull/1]).

-define(VALID_PLATFORMS, [<<"windows">>, <<"macos">>, <<"linux">>]).

%% ------------------------------------------------------------------
%% Business logic for update checks and payload delivery
%% ------------------------------------------------------------------

-spec handle_check(binary()) -> binary().
handle_check(Payload) ->
    try
        Req = jsx:decode(Payload, [return_maps]),
        Current = maps:get(<<"current_version">>, Req, <<"0.0.0">>),
        Platform = maps:get(<<"platform">>, Req, <<"generic">>),

        case lists:member(Platform, ?VALID_PLATFORMS) of
            false ->
                jsx:encode(#{
                    <<"error">> => <<"unsupported_platform">>,
                    <<"supported">> => ?VALID_PLATFORMS
                });
            true ->
                case latest_version_from_cmake() of
                    {ok, Latest} ->
                        case compare_versions(Current, Latest) of
                            lt ->
                                jsx:encode(#{
                                    <<"available">> => true,
                                    <<"latest_version">> => Latest
                                });
                            _ ->
                                jsx:encode(#{<<"available">> => false})
                        end;
                    {error, Reason} ->
                        jsx:encode(#{<<"error">> => iolist_to_binary(io_lib:format("~p", [Reason]))})
                end
        end
    catch
        _:_ ->
            jsx:encode(#{<<"error">> => <<"invalid_request">>})
    end.

-spec handle_pull(binary()) -> binary().
handle_pull(Payload) ->
    try
        Req = jsx:decode(Payload, [return_maps]),
        Url = maps:get(<<"url">>, Req, <<>>),
        Offset = maps:get(<<"offset">>, Req, 0),

        case fetch_chunk(Url, Offset) of
            {chunk, Data, More} ->
                jsx:encode(#{
                    <<"data">> => base64:encode(Data),
                    <<"more">> => More,
                    <<"offset">> => Offset + byte_size(Data)
                });
            not_found ->
                jsx:encode(#{<<"error">> => <<"not_found">>})
        end
    catch
        _:_ ->
            jsx:encode(#{<<"error">> => <<"invalid_request">>})
    end.

%% ------------------------------------------------------------------
%% Version extraction from CMakeLists.txt
%% ------------------------------------------------------------------

-spec latest_version_from_cmake() -> {ok, binary()} | {error, term()}.
latest_version_from_cmake() ->
    File = application:get_env(server, version_file, "/opt/seelie-server/CMakeLists.txt"),
    case file:read_file(File) of
        {ok, Bin} ->
            case extract_cmake_version(Bin) of
                {match, Vsn} -> {ok, Vsn};
                nomatch -> {error, version_not_found}
            end;
        {error, Reason} ->
            {error, Reason}
    end.

-spec extract_cmake_version(binary()) -> {match, binary()} | nomatch.
extract_cmake_version(Bin) ->
    %% Matches: project(Seelie VERSION 1.2.0 ...)
    case re:run(Bin, <<"project\\s*\\([^)]*VERSION\\s+([0-9]+\\.[0-9]+\\.[0-9]+)">>, [
        caseless, {capture, all_but_first, binary}
    ]) of
        {match, [Vsn]} -> {match, Vsn};
        _ -> nomatch
    end.

%% ------------------------------------------------------------------
%% Version comparison (simplified semantic)
%% ------------------------------------------------------------------

-spec compare_versions(binary(), binary()) -> eq | lt | gt.
compare_versions(A, B) ->
    Av = [binary_to_integer(X) || X <- binary:split(A, <<".">>, [global])],
    Bv = [binary_to_integer(X) || X <- binary:split(B, <<".">>, [global])],
    compare_int_lists(Av, Bv).

compare_int_lists([], []) -> eq;
compare_int_lists([], [_|_]) -> lt;
compare_int_lists([_|_], []) -> gt;
compare_int_lists([A|_As], [B|_Bs]) when A < B -> lt;
compare_int_lists([A|_As], [B|_Bs]) when A > B -> gt;
compare_int_lists([_|As], [_|Bs]) -> compare_int_lists(As, Bs).

%% ------------------------------------------------------------------
%% Internal / Stubbed chunk storage
%% ------------------------------------------------------------------

fetch_chunk(_Url, _Offset) ->
    %% TODO: replace with real file chunking per platform
    {chunk, <<"PAYLOAD">>, false}.
