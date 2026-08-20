
#include <gtest/gtest.h>
#include "dwn/auth.hpp"
#include "dwn/did.hpp"

using namespace dwn;

TEST(DID, Parse){
    auto r = parseDid("did:example:123");
    EXPECT_TRUE(r.ok);
    EXPECT_EQ(r.did.method, "example");
    auto r2 = parseDid("did:key:z6Mkj3PUd1v4a4j3m4V2g6");
    EXPECT_TRUE(r2.ok);
    EXPECT_FALSE(validateDid("not-a-did"));
}

TEST(DID, Normalize){
    auto n = normalizeDid("did:EXAMPLE:123");
    EXPECT_EQ(n, "did:example:123");
}

TEST(DID, KeyExtract){
    // Example did:key for Ed25519 from spec: z6Mk... - we'll test invalid returns nullopt
    auto pub = extract_ed25519_pubkey_from_did_key("did:key:z6MkiTBz9ymR7K2oX3Q6X7fake");
    // invalid base58 length should fail, so nullopt expected
    EXPECT_FALSE(pub.has_value()); // just check no crash
}

TEST(Auth, OwnerOnlyDevMode){
    AuthVerifier auth;
    Message msg;
    msg.targetDid="did:example:123";
    msg.descriptor.interfaceName="Records";
    msg.descriptor.method="Write";
    msg.authorization.signatures={}; // empty -> dev mode allows did:example
    auto res = auth.verifyAuthorization(msg, "did:example:123");
    EXPECT_TRUE(res.authorized);
    auto res2 = auth.verifyAuthorization(msg, "did:key:z6Mk123");
    EXPECT_FALSE(res2.authorized); // did:key requires signature
}
