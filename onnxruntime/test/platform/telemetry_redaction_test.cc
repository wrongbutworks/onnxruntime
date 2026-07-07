// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/platform/telemetry_redaction.h"

#include <string>

#include "gtest/gtest.h"

namespace onnxruntime {
namespace test {

TEST(TelemetryRedactionTest, EmptyAndNoPath) {
  EXPECT_EQ(ScrubErrorMessage(""), "");
  EXPECT_EQ(ScrubErrorMessage("no path here"), "no path here");
  EXPECT_EQ(ScrubErrorMessage("error code 13"), "error code 13");
}

TEST(TelemetryRedactionTest, PosixPathReplacedWithPlaceholder) {
  // The sensitive prefix (home dir + user name) is redacted, but the last two segments are kept.
  EXPECT_EQ(ScrubErrorMessage("Load model from /home/alice/models/foo.onnx failed"),
            "Load model from [path]/models/foo.onnx failed");
  // The username must not survive.
  EXPECT_EQ(ScrubErrorMessage("/home/alice/models/foo.onnx").find("alice"), std::string::npos);
}

TEST(TelemetryRedactionTest, WindowsDriveAndUncReplaced) {
  EXPECT_EQ(ScrubErrorMessage("Load C:\\Users\\bob\\m.onnx failed"), "Load [path]\\m.onnx failed");
  EXPECT_EQ(ScrubErrorMessage("open D:/data/secret/model.onnx"), "open [path]/secret/model.onnx");
  EXPECT_EQ(ScrubErrorMessage("from \\\\server\\share\\dir\\weights.bin done"),
            "from [path]\\dir\\weights.bin done");
  EXPECT_EQ(ScrubErrorMessage("Load C:\\Users\\bob\\m.onnx failed").find("bob"), std::string::npos);
}

TEST(TelemetryRedactionTest, PathsWithSpacesDoNotLeakUsername) {
  // A space splits the path into two tokens: "C:\Users\First" (all prefix -> "[path]") and
  // "Last\model.onnx" (single separator -> only the file name is kept). Neither name half survives.
  EXPECT_EQ(ScrubErrorMessage("Load C:\\Users\\First Last\\model.onnx failed"),
            "Load [path] [path]\\model.onnx failed");
  const std::string spaced = ScrubErrorMessage("Load C:\\Users\\First Last\\model.onnx failed");
  EXPECT_EQ(spaced.find("First"), std::string::npos);
  EXPECT_EQ(spaced.find("Last"), std::string::npos);
}

TEST(TelemetryRedactionTest, KeepsLastTwoSegmentsButGuardsHomeUsername) {
  // Deep path: the last two segments (parent dir + file) are kept.
  EXPECT_EQ(ScrubErrorMessage("/home/alice/proj/rn/model.onnx"), "[path]/rn/model.onnx");
  EXPECT_EQ(ScrubErrorMessage("/data/models/rn/model.onnx"), "[path]/rn/model.onnx");
  // File directly under the home dir: only the file name is kept, so the user name is never the
  // second-to-last (retained) segment.
  EXPECT_EQ(ScrubErrorMessage("/home/alice/model.onnx"), "[path]/model.onnx");
  EXPECT_EQ(ScrubErrorMessage("C:\\Users\\bob\\model.onnx"), "[path]\\model.onnx");
  EXPECT_EQ(ScrubErrorMessage("/home/alice/model.onnx").find("alice"), std::string::npos);
  EXPECT_EQ(ScrubErrorMessage("C:\\Users\\bob\\model.onnx").find("bob"), std::string::npos);
}

TEST(TelemetryRedactionTest, MultiSegmentRelativeAndUrlReplaced) {
  // A token with 2+ "/x" segments (incl. URLs) is treated as a path; the last two segments are kept.
  EXPECT_EQ(ScrubErrorMessage("a/b/c"), "[path]/b/c");
  EXPECT_EQ(ScrubErrorMessage("see https://example.com/a/b/c for details"), "see [path]/b/c for details");
  // Home prefixes are guarded even when embedded in a larger token, so the user name is redacted.
  EXPECT_EQ(ScrubErrorMessage("input:/home/alice/secret/m.onnx"), "[path]/secret/m.onnx");
  EXPECT_EQ(ScrubErrorMessage("file:///home/alice/secret/model.onnx"), "[path]/secret/model.onnx");
  EXPECT_EQ(ScrubErrorMessage("input:/home/alice/secret/m.onnx").find("alice"), std::string::npos);
  EXPECT_EQ(ScrubErrorMessage("~/.config/app/x"), "[path]/app/x");
}

TEST(TelemetryRedactionTest, SingleSegmentAndNonPathSlashesKept) {
  EXPECT_EQ(ScrubErrorMessage("models/foo.onnx"), "models/foo.onnx");
  EXPECT_EQ(ScrubErrorMessage("ratio 3/4 and and/or"), "ratio 3/4 and and/or");
}

TEST(TelemetryRedactionTest, LengthIsCappedAfterScrub) {
  const std::string long_msg(300, 'x');
  EXPECT_EQ(ScrubErrorMessage(long_msg).size(), kMaxTelemetryErrorMessageLength);
  EXPECT_LE(ScrubErrorMessage("short").size(), kMaxTelemetryErrorMessageLength);
}

}  // namespace test
}  // namespace onnxruntime
