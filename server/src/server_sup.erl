-module(server_sup).
-behaviour(supervisor).

-export([start_link/0]).
-export([init/1]).

-define(SERVER, ?MODULE).

start_link() ->
    supervisor:start_link({local, ?SERVER}, ?MODULE, []).

init([]) ->
    Port = application:get_env(server, udp_port, 9340),
    Workers = application:get_env(server, udp_workers, 4),

    Children = [
        {udp_server,
            {udp_server, start_link, [Port, Workers]},
            permanent, 5000, worker, [udp_server]}
    ],

    SupFlags = #{strategy => one_for_one, intensity => 5, period => 10},
    {ok, {SupFlags, Children}}.
