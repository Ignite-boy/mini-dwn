# Mini-DWN — C++ Decentralized Web Node (MVP)

Standards-aware, modular DWN server in C++20 for Milan.

## Architecture
HTTP Client -> JSON-RPC (Beast) -> DWN Router (dwn.processMessage) -> Records Handler / DID Resolver / Auth Verifier -> Storage Interface -> PostgreSQL

## Quick Start
Docker:
```
docker-compose -f docker/docker-compose.yml up --build
```
Local:
```
cmake -S . -B build
cmake --build build
./build/mini-dwn
```

## API
POST /json-rpc
GET /health /info /metrics /version
WebSocket ws://host:port/

### dwn.processMessage example
See examples/milan_client.js

## Security
TLS, rate limiting (120/min), request size limits, SQL parameterization, Ed25519 sig verification via libsodium, owner-only MVP, tenant isolation at storage layer.

## DB Schema
See migrations/001_initial.sql - tenants, records, record_data, events.

## Milan Integration
Milan Node.js backend calls POST https://dwn.milanlife.in/json-rpc with dwn.processMessage.

## Testing
```
cmake -S . -B build -DBUILD_TESTS=ON
./build/dwn_tests
```

Full README with curl examples is in docs/README_FULL.md (placeholder due to throttling).

## Compatibility Rule
This is Mini-DWN MVP, not full spec. Keep protocol code isolated for future compat.
