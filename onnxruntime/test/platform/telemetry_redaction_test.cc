// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "core/platform/telemetry_redaction.h"

#include <string>

#include "gtest/gtest.h"

namespace onnxruntime {
namespace test {

TEST(TelemetryRedactionTest, EmptyAndNoPath) {
  EXPECT_EQ(ScrubStringForTelemetry(""), "");
  EXPECT_EQ(ScrubStringForTelemetry("no path here"), "no path here");
  EXPECT_EQ(ScrubStringForTelemetry("error code 13"), "error code 13");
}

TEST(TelemetryRedactionTest, KeepsLastTwoSegmentsAndGuardsHomeUsername) {
  // The last two segments (parent directory + file name) are kept for debuggability, whether or not
  // the path is under a home directory or is embedded in a surrounding sentence.
  EXPECT_EQ(ScrubStringForTelemetry("/home/alice/proj/rn/model.onnx"), "[path]/rn/model.onnx");
  EXPECT_EQ(ScrubStringForTelemetry("/data/models/rn/model.onnx"), "[path]/rn/model.onnx");
  EXPECT_EQ(ScrubStringForTelemetry("Load model from /home/alice/models/foo.onnx failed"),
            "Load model from [path]/models/foo.onnx failed");
  // A file directly under the home directory keeps only its file name, so the user name (the
  // second-to-last segment) is never retained -- on POSIX or Windows.
  EXPECT_EQ(ScrubStringForTelemetry("/home/alice/model.onnx"), "[path]/model.onnx");
  EXPECT_EQ(ScrubStringForTelemetry("C:\\Users\\bob\\model.onnx"), "[path]\\model.onnx");
}

TEST(TelemetryRedactionTest, GuardsHomeUsernameAcrossCaseAndSeparatorVariants) {
  // Home directories are case-insensitive on Windows/macOS and a single path may mix '/' and '\'.
  // The user name must never be the retained second-to-last segment regardless of the case or
  // separator used at the home boundary -- each exact result below contains no user name.
  EXPECT_EQ(ScrubStringForTelemetry("C:\\UsErS\\alice\\model.onnx"), "[path]\\model.onnx");
  EXPECT_EQ(ScrubStringForTelemetry("C:\\Users/bob\\model.onnx"), "[path]\\model.onnx");
  EXPECT_EQ(ScrubStringForTelemetry("C:/Users\\bob\\model.onnx"), "[path]\\model.onnx");
  EXPECT_EQ(ScrubStringForTelemetry("/UsErS/alice/model.onnx"), "[path]/model.onnx");
  // A deep mixed-separator path still keeps the last two segments while guarding the user name.
  EXPECT_EQ(ScrubStringForTelemetry("C:\\Users/bob/proj\\model.onnx"), "[path]/proj\\model.onnx");
}

TEST(TelemetryRedactionTest, GuardsHomeUsernameForPathEquivalentSpellings) {
  // Redundant separators and "." segments (common from sloppy path joins) must not let the user
  // name slip into the kept tail: /home//user, C:\Users\\user, and /home/./user all reduce like
  // the canonical form.
  EXPECT_EQ(ScrubStringForTelemetry("/home//alice/model.onnx"), "[path]/model.onnx");
  EXPECT_EQ(ScrubStringForTelemetry("C:\\Users\\\\alice\\model.onnx"), "[path]\\model.onnx");
  EXPECT_EQ(ScrubStringForTelemetry("/home/./alice/model.onnx"), "[path]/model.onnx");
}

TEST(TelemetryRedactionTest, TrailingSeparatorKeepsTwoRealSegments) {
  // A path ending in a separator keeps the last two REAL segments -- the trailing separator is not
  // treated as an empty final segment -- while the home user name stays redacted.
  EXPECT_EQ(ScrubStringForTelemetry("/data/models/secret/"), "[path]/models/secret/");
  EXPECT_EQ(ScrubStringForTelemetry("x/y/z/"), "[path]/y/z/");
  EXPECT_EQ(ScrubStringForTelemetry("/home/alice/proj/sub/"), "[path]/proj/sub/");
  EXPECT_EQ(ScrubStringForTelemetry("/home/alice/models/"), "[path]/models/");
  EXPECT_EQ(ScrubStringForTelemetry("/home/alice/"), "[path]/");
}

TEST(TelemetryRedactionTest, WindowsDriveAndUncReplaced) {
  EXPECT_EQ(ScrubStringForTelemetry("Load C:\\proj\\bin\\m.onnx failed"), "Load [path]\\bin\\m.onnx failed");
  EXPECT_EQ(ScrubStringForTelemetry("open D:/data/secret/model.onnx"), "open [path]/secret/model.onnx");
  EXPECT_EQ(ScrubStringForTelemetry("from \\\\server\\share\\dir\\weights.bin done"),
            "from [path]\\dir\\weights.bin done");
}

TEST(TelemetryRedactionTest, PathsWithSpacesDoNotLeakUsername) {
  // A space splits the path into two tokens: "C:\Users\First" (all prefix -> "[path]") and
  // "Last\model.onnx" (single separator -> only the file name is kept). Neither name half survives.
  const std::string spaced = ScrubStringForTelemetry("Load C:\\Users\\First Last\\model.onnx failed");
  EXPECT_EQ(spaced, "Load [path] [path]\\model.onnx failed");
  EXPECT_EQ(spaced.find("First"), std::string::npos);
  EXPECT_EQ(spaced.find("Last"), std::string::npos);
}

TEST(TelemetryRedactionTest, MultiSegmentRelativeAndUrlReplaced) {
  // A token with 2+ "/x" segments (incl. URLs) is treated as a path; the last two segments are kept.
  EXPECT_EQ(ScrubStringForTelemetry("a/b/c"), "[path]/b/c");
  EXPECT_EQ(ScrubStringForTelemetry("see https://example.com/a/b/c for details"), "see [path]/b/c for details");
  // Home prefixes are guarded even when embedded in a larger token, so the user name is redacted.
  EXPECT_EQ(ScrubStringForTelemetry("input:/home/alice/secret/m.onnx"), "[path]/secret/m.onnx");
  EXPECT_EQ(ScrubStringForTelemetry("file:///home/alice/secret/model.onnx"), "[path]/secret/model.onnx");
  EXPECT_EQ(ScrubStringForTelemetry("~/.config/app/x"), "[path]/app/x");
}

TEST(TelemetryRedactionTest, SingleSegmentAndNonPathSlashesKept) {
  EXPECT_EQ(ScrubStringForTelemetry("models/foo.onnx"), "models/foo.onnx");
  EXPECT_EQ(ScrubStringForTelemetry("ratio 3/4 and and/or"), "ratio 3/4 and and/or");
}

TEST(TelemetryRedactionTest, LengthIsCappedAfterScrub) {
  const std::string long_msg(300, 'x');
  EXPECT_EQ(ScrubStringForTelemetry(long_msg).size(), kMaxTelemetryStringLength);
  EXPECT_LE(ScrubStringForTelemetry("short").size(), kMaxTelemetryStringLength);
}

}  // namespace test
}  // namespace onnxruntime
