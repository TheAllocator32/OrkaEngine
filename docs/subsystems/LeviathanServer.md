# Leviathan Server

`LeviathanServer` supports programmatic and WhaleML lobby configuration. Configure it before calling `start()`.

```cpp
LeviathanServer server;
server.configureLobbiesFromWhaleML("server_config.whaleml");
server.start();
```

`lobbies.count` is required. `lobbies.default` supplies settings for every lobby, and `lobbies.lobbyN` overrides an individual zero-based lobby.

```whaleml
lobbies
    count : 150

    default.maxPlayers : 8
    default.countdownStartPlayerCount : 6
    default.countdownDuration : 20.0
    default.allowManualStart : true
    default.manualStartVoteThresholdPercent : 60.0

    // A smaller lobby with a shorter countdown.
    lobby0.maxPlayers : 4
    lobby0.countdownStartPlayerCount : 2
    lobby0.countdownDuration : 10.0
```

Missing settings use `LeviathanLobbyConfiguration` defaults. Values are normalized the same way as programmatic configuration.
