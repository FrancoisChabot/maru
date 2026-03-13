#include "utest.h"
#include "ime_utils.h"
#include <string.h>

UTEST(TextTest, ApplyTextEditCommitUtf8AcceptsWholeCodepointEdits) {
    char buffer[16] = "a\xC3\xA9z";
    uint32_t length = 4u;
    uint32_t cursor = 3u;
    MARU_TextEditCommittedEvent commit = {0};
    commit.delete_before_bytes = 2u;
    commit.committed_utf8 = "x";
    commit.committed_length_bytes = 1u;

    EXPECT_TRUE(maru_applyTextEditCommitUtf8(buffer, sizeof(buffer), &length, &cursor, &commit));
    EXPECT_EQ(length, (uint32_t)3u);
    EXPECT_EQ(cursor, (uint32_t)2u);
    EXPECT_TRUE(strcmp(buffer, "axz") == 0);
}

UTEST(TextTest, ApplyTextEditCommitUtf8RejectsSplitCodepointDeletion) {
    char buffer[16] = "a\xC3\xA9z";
    char before[16];
    uint32_t length = 4u;
    uint32_t cursor = 3u;
    MARU_TextEditCommittedEvent commit = {0};
    memcpy(before, buffer, sizeof(buffer));
    commit.delete_before_bytes = 1u;

    EXPECT_FALSE(maru_applyTextEditCommitUtf8(buffer, sizeof(buffer), &length, &cursor, &commit));
    EXPECT_EQ(length, (uint32_t)4u);
    EXPECT_EQ(cursor, (uint32_t)3u);
    EXPECT_TRUE(memcmp(buffer, before, sizeof(buffer)) == 0);
}

UTEST(TextTest, ApplyTextEditCommitUtf8RejectsInvalidUtf8Insertion) {
    char buffer[16] = "abc";
    const char invalid_utf8[2] = {(char)0xC3, (char)0x28};
    uint32_t length = 3u;
    uint32_t cursor = 3u;
    MARU_TextEditCommittedEvent commit = {0};
    commit.committed_utf8 = invalid_utf8;
    commit.committed_length_bytes = 2u;

    EXPECT_FALSE(maru_applyTextEditCommitUtf8(buffer, sizeof(buffer), &length, &cursor, &commit));
    EXPECT_EQ(length, (uint32_t)3u);
    EXPECT_EQ(cursor, (uint32_t)3u);
    EXPECT_TRUE(strcmp(buffer, "abc") == 0);
}
