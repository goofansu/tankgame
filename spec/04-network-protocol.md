# Network Protocol and Implementation

## Architecture Overview

**Client-Server model:**
- Dedicated server (can be hosted by a player)
- Server is authoritative for game state
- Clients send inputs, receive state updates
- Client-side prediction for responsive feel
- Server reconciliation to correct mispredictions

## Transport Layer: QUIC via picoquic

We use **QUIC** (RFC 9000) as our transport protocol, implemented via [picoquic](https://github.com/private-octopus/picoquic).

### Why QUIC?

| Feature | Benefit for Game |
|---------|------------------|
| **Datagrams (RFC 9221)** | Unreliable, low-latency game state - like UDP but encrypted |
| **Multiplexed streams** | Reliable chat/events without head-of-line blocking |
| **Built-in encryption** | TLS 1.3, no separate security layer needed |
| **0-RTT connection** | Fast reconnects |
| **Connection migration** | Survives IP changes (mobile, WiFi handoff) |
| **WebTransport** | Browser support via HTTP/3 |

### picoquic Overview

- Clean C implementation by Christian Huitema (QUIC spec co-author)
- ~50k LOC, well-maintained
- Dependencies: picotls + OpenSSL (or minicrypto for minimal builds)
- MIT license

### QUIC Channel Design

We use two types of QUIC channels:

1. **Datagrams** (unreliable, unordered) - for real-time game data:
   - Client inputs
   - Game state deltas
   - Entity positions

2. **Streams** (reliable, ordered) - for important events:
   - Stream 0: Connection handshake, player join/leave
   - Stream 2: Chat messages
   - Stream 4: Game events (kills, captures, round start/end)

```
┌─────────────────────────────────────────────────┐
│                   QUIC Connection               │
├─────────────────────────────────────────────────┤
│  Datagrams (unreliable)                         │
│  ├─ Client Input                                │
│  ├─ Game State Delta                            │
│  └─ Entity Updates                              │
├─────────────────────────────────────────────────┤
│  Stream 0 (reliable) - Control                  │
│  ├─ Connect Request/Accept                      │
│  ├─ Player Join/Leave                           │
│  └─ Full Game State (on join)                   │
├─────────────────────────────────────────────────┤
│  Stream 2 (reliable) - Chat                     │
│  └─ Chat Messages                               │
├─────────────────────────────────────────────────┤
│  Stream 4 (reliable) - Events                   │
│  ├─ Kill Events                                 │
│  ├─ Flag Captured                               │
│  └─ Round Start/End                             │
└─────────────────────────────────────────────────┘
```

## picoquic Integration

### Wrapper API

We wrap picoquic with a thin game-specific layer:

```c
// Core QUIC context (wraps picoquic_quic_t)
typedef struct pz_net_context {
    picoquic_quic_t*     quic;
    bool                 is_server;
    uint64_t             current_time;
    
    // Callbacks
    pz_net_callbacks     callbacks;
    void*                userdata;
} pz_net_context;

// Connection (wraps picoquic_cnx_t)
typedef struct pz_net_conn {
    picoquic_cnx_t*      cnx;
    pz_net_context*      ctx;
    int                  client_id;
    pz_conn_state        state;
    
    // Stats
    float                rtt;
    uint64_t             bytes_sent;
    uint64_t             bytes_recv;
} pz_net_conn;

// Callbacks from network layer to game
typedef struct pz_net_callbacks {
    void (*on_connected)(pz_net_conn* conn, void* userdata);
    void (*on_disconnected)(pz_net_conn* conn, void* userdata);
    void (*on_datagram)(pz_net_conn* conn, const uint8_t* data, size_t len, void* userdata);
    void (*on_stream_data)(pz_net_conn* conn, uint64_t stream_id, const uint8_t* data, size_t len, bool fin, void* userdata);
} pz_net_callbacks;

// Server API
pz_net_context* pz_net_server_create(int port, const char* cert_file, const char* key_file);
void            pz_net_server_update(pz_net_context* ctx);  // Call each frame

// Client API  
pz_net_context* pz_net_client_create(void);
pz_net_conn*    pz_net_client_connect(pz_net_context* ctx, const char* host, int port);
void            pz_net_client_update(pz_net_context* ctx);  // Call each frame

// Sending
void pz_net_send_datagram(pz_net_conn* conn, const uint8_t* data, size_t len);
void pz_net_send_stream(pz_net_conn* conn, uint64_t stream_id, const uint8_t* data, size_t len, bool fin);

// Cleanup
void pz_net_conn_close(pz_net_conn* conn);
void pz_net_context_destroy(pz_net_context* ctx);
```

### picoquic Callback Handler

```c
// Main callback from picoquic - routes to our callbacks
static int pz_picoquic_callback(picoquic_cnx_t* cnx,
    uint64_t stream_id, uint8_t* bytes, size_t length,
    picoquic_call_back_event_t event, void* callback_ctx, void* stream_ctx)
{
    pz_net_conn* conn = (pz_net_conn*)callback_ctx;
    pz_net_context* ctx = conn->ctx;
    
    switch (event) {
        case picoquic_callback_stream_data:
        case picoquic_callback_stream_fin:
            if (ctx->callbacks.on_stream_data) {
                ctx->callbacks.on_stream_data(conn, stream_id, bytes, length,
                    event == picoquic_callback_stream_fin, ctx->userdata);
            }
            break;
            
        case picoquic_callback_datagram:
            if (ctx->callbacks.on_datagram) {
                ctx->callbacks.on_datagram(conn, bytes, length, ctx->userdata);
            }
            break;
            
        case picoquic_callback_close:
        case picoquic_callback_application_close:
            if (ctx->callbacks.on_disconnected) {
                ctx->callbacks.on_disconnected(conn, ctx->userdata);
            }
            break;
            
        // ... other events
    }
    return 0;
}
```

## Game Protocol

Binary protocol, little-endian. All messages have a 1-byte type header.

### Message Types

```c
typedef enum pz_msg_type {
    // Via Stream 0 (reliable control)
    PZ_MSG_CONNECT_REQUEST = 1,
    PZ_MSG_CONNECT_ACCEPT,
    PZ_MSG_CONNECT_DENY,
    PZ_MSG_PLAYER_JOIN,
    PZ_MSG_PLAYER_LEAVE,
    PZ_MSG_FULL_STATE,
    
    // Via Datagrams (unreliable game data)
    PZ_MSG_CLIENT_INPUT = 32,
    PZ_MSG_GAME_DELTA,
    
    // Via Stream 2 (reliable chat)
    PZ_MSG_CHAT = 64,
    
    // Via Stream 4 (reliable events)
    PZ_MSG_EVENT_KILL = 96,
    PZ_MSG_EVENT_FLAG_TAKEN,
    PZ_MSG_EVENT_FLAG_CAPTURED,
    PZ_MSG_EVENT_FLAG_RETURNED,
    PZ_MSG_EVENT_ZONE_CAPTURED,
    PZ_MSG_EVENT_ROUND_START,
    PZ_MSG_EVENT_ROUND_END,
} pz_msg_type;
```

### Key Messages

**Connect Request (Client → Server, Stream 0):**
```c
typedef struct pz_msg_connect_request {
    uint8_t  type;              // PZ_MSG_CONNECT_REQUEST
    uint8_t  protocol_version;
    uint8_t  name_len;
    char     name[32];          // Player name (up to 32 chars)
    uint8_t  preferred_team;    // 0 = auto-assign
} pz_msg_connect_request;
```

**Connect Accept (Server → Client, Stream 0):**
```c
typedef struct pz_msg_connect_accept {
    uint8_t  type;              // PZ_MSG_CONNECT_ACCEPT
    uint8_t  client_id;
    uint8_t  team;
    uint32_t entity_id;         // Your tank's entity ID
    uint32_t server_tick;
    // Followed by full game state
} pz_msg_connect_accept;
```

**Client Input (Client → Server, Datagram):**
```c
typedef struct pz_msg_client_input {
    uint8_t  type;              // PZ_MSG_CLIENT_INPUT
    uint32_t input_sequence;
    uint32_t server_tick_ack;   // Last server tick client processed
    uint8_t  buttons;           // Bitfield
    uint16_t turret_angle;      // 0-65535 = 0-360°
} pz_msg_client_input;
```

Buttons bitfield:
```c
enum {
    PZ_INPUT_FORWARD  = 1 << 0,
    PZ_INPUT_BACKWARD = 1 << 1,
    PZ_INPUT_LEFT     = 1 << 2,
    PZ_INPUT_RIGHT    = 1 << 3,
    PZ_INPUT_FIRE     = 1 << 4,
    PZ_INPUT_MINE     = 1 << 5,
    PZ_INPUT_ITEM     = 1 << 6,
};
```

**Game Delta (Server → Client, Datagram):**
```c
typedef struct pz_msg_game_delta {
    uint8_t  type;              // PZ_MSG_GAME_DELTA
    uint32_t server_tick;
    uint32_t your_last_input;   // For client reconciliation
    uint8_t  tank_count;
    // Followed by: pz_tank_state[tank_count]
    uint8_t  projectile_count;
    // Followed by: pz_projectile_state[projectile_count]
    uint8_t  mine_count;
    // Followed by: pz_mine_state[mine_count]
} pz_msg_game_delta;

typedef struct pz_tank_state {
    uint32_t entity_id;
    int16_t  x, y;              // Fixed-point (8.8)
    uint16_t angle;             // 0-65535 = 0-360°
    uint16_t turret_angle;
    uint8_t  hp;
    uint8_t  weapon;
    uint8_t  ammo;
    uint8_t  flags;             // PZ_TANK_FLAG_*
} pz_tank_state;

enum {
    PZ_TANK_FLAG_MOVING  = 1 << 0,
    PZ_TANK_FLAG_FIRING  = 1 << 1,
    PZ_TANK_FLAG_DEAD    = 1 << 2,
    PZ_TANK_FLAG_INVULN  = 1 << 3,
};
```

**Game Event (Server → Client, Stream 4):**
```c
typedef struct pz_msg_event_kill {
    uint8_t  type;              // PZ_MSG_EVENT_KILL
    uint32_t tick;
    uint8_t  killer_client_id;
    uint8_t  victim_client_id;
    uint8_t  weapon;
} pz_msg_event_kill;
```

## Tick Rate and Timing

- **Server tick rate:** 60 Hz (16.67ms)
- **Client send rate:** 60 Hz (match server)
- **Server broadcast rate:** 20-30 Hz (every 2-3 ticks)
- **Input buffer:** Server keeps last N inputs per client (handles jitter)

### Time Synchronization

QUIC gives us RTT measurements. We use this to:
1. Estimate server time on client
2. Set interpolation delay (render_tick = server_tick - delay)
3. Adjust input timing for prediction

```c
// On receiving game delta:
float half_rtt = conn->rtt / 2.0f;
client->server_time_estimate = local_time + half_rtt;
client->render_tick = server_tick - (uint32_t)(INTERP_DELAY_MS / TICK_MS);
```

## Client-Side Prediction

```c
typedef struct pz_prediction_state {
    // Ring buffer of past inputs
    pz_msg_client_input input_history[PZ_INPUT_HISTORY_SIZE];
    int                 input_head;
    
    // Ring buffer of past predicted states  
    pz_tank_state       state_history[PZ_INPUT_HISTORY_SIZE];
    
    // Reconciliation
    uint32_t            last_confirmed_input;
} pz_prediction_state;

void pz_prediction_record_input(pz_prediction_state* pred, 
    pz_msg_client_input* input, pz_tank_state* predicted_state);
void pz_prediction_reconcile(pz_prediction_state* pred, 
    pz_tank_state* server_state, uint32_t server_input_seq);
```

Reconciliation flow:
1. Receive server state with `your_last_input` sequence
2. Compare server state to our predicted state for that input
3. If mismatch: reset to server state, replay inputs from `your_last_input+1` to now
4. Smoothly blend if difference is small (avoid visual pops)

## Entity Interpolation

For other players' tanks and projectiles:

```c
typedef struct pz_interp_buffer {
    struct {
        uint32_t      tick;
        pz_tank_state state;
    } snapshots[PZ_INTERP_BUFFER_SIZE];
    int head;
    int count;
} pz_interp_buffer;

pz_tank_state pz_interp_entity(pz_interp_buffer* buf, uint32_t render_tick);
```

Render tick is typically 100ms behind server tick to allow for network jitter.

## Server Structure

```c
typedef struct pz_server {
    // Network
    pz_net_context*  net;
    
    // Clients
    pz_server_client clients[PZ_MAX_CLIENTS];
    int              client_count;
    
    // Game
    pz_game*         game;
    uint32_t         tick;
    
    // Timing
    double           tick_accumulator;
    double           broadcast_accumulator;
} pz_server;

typedef struct pz_server_client {
    pz_net_conn*     conn;
    int              client_id;
    int              tank_entity_id;
    char             name[32];
    int              team;
    
    // Input buffer (handles network jitter)
    pz_msg_client_input input_buffer[PZ_INPUT_BUFFER_SIZE];
    int              input_write;
    int              input_read;
    uint32_t         last_input_seq;
    
    // For delta compression (future optimization)
    pz_tank_state    last_sent_state;
} pz_server_client;

void pz_server_init(pz_server* server, int port, const char* cert, const char* key);
void pz_server_update(pz_server* server, float dt);
void pz_server_shutdown(pz_server* server);
```

## Client Structure

```c
typedef struct pz_client {
    // Network
    pz_net_context*      net;
    pz_net_conn*         conn;
    
    // State
    pz_client_state      state;       // DISCONNECTED, CONNECTING, CONNECTED, IN_GAME
    int                  my_client_id;
    int                  my_tank_id;
    
    // Prediction
    pz_prediction_state  prediction;
    
    // Interpolation (for other entities)
    pz_interp_buffer     entity_interp[PZ_MAX_ENTITIES];
    
    // Timing
    uint32_t             server_tick;
    uint32_t             render_tick;
    uint32_t             local_input_seq;
    double               server_time_estimate;
} pz_client;

void pz_client_init(pz_client* client);
void pz_client_connect(pz_client* client, const char* host, int port);
void pz_client_disconnect(pz_client* client);
void pz_client_update(pz_client* client, float dt);
void pz_client_send_input(pz_client* client, pz_msg_client_input* input);
void pz_client_shutdown(pz_client* client);
```

## WebTransport for Browser

For web builds, picoquic supports WebTransport over HTTP/3. The game protocol stays identical - only the connection setup differs.

```c
#ifdef __EMSCRIPTEN__
    // Browser: use WebTransport API (JavaScript interop)
    // Connect to: https://server:port/.well-known/webtransport
    #include "pz_net_webtransport.h"
#else
    // Native: use picoquic directly
    #include "pz_net_quic.h"
#endif
```

For Emscripten builds, we'll need a small JavaScript shim that uses the browser's WebTransport API and calls back into our C code.

## Build Configuration

picoquic dependencies in CMake:

```cmake
# Fetch picoquic and dependencies
include(FetchContent)

FetchContent_Declare(
    picotls
    GIT_REPOSITORY https://github.com/h2o/picotls.git
    GIT_TAG master
)

FetchContent_Declare(
    picoquic
    GIT_REPOSITORY https://github.com/private-octopus/picoquic.git
    GIT_TAG master
)

FetchContent_MakeAvailable(picotls picoquic)

target_link_libraries(tankgame PRIVATE picoquic-core)
```

## Security

QUIC/TLS 1.3 provides:
- **Encryption:** All traffic encrypted by default
- **Authentication:** Server certificate verification
- **Replay protection:** Built into QUIC
- **Connection ID:** Prevents IP spoofing

Additional game-level security:
- Server validates all inputs (bounds checking, rate limiting)
- Server is authoritative (clients can't cheat game state)
- Input sequence numbers prevent replay within session
