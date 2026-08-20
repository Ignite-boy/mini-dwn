#!/bin/bash
curl http://localhost:3000/health
curl http://localhost:3000/info

DATA=$(echo -n '{"hello":"milan"}' | base64 | tr '+/' '-_' | tr -d '=')
curl -X POST http://localhost:3000/json-rpc -H "Content-Type: application/json" -d "{
  \"jsonrpc\":\"2.0\",\"id\":\"1\",\"method\":\"dwn.processMessage\",
  \"params\":{
    \"target\":\"did:example:123\",
    \"message\":{
      \"descriptor\":{\"interface\":\"Records\",\"method\":\"Write\",\"dataFormat\":\"application/json\",\"dateCreated\":\"2024-01-01T00:00:00Z\",\"dateModified\":\"2024-01-01T00:00:00Z\",\"recordId\":\"rec1\"},
      \"authorization\":{\"payload\":\"e30\",\"signatures\":[]}
    },
    \"encodedData\":\"$DATA\"
  }
}"
