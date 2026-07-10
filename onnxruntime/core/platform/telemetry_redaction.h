// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#pragma once

#include <cctype>
#include <string>
#include <string_view>

namespace onnxruntime {
namespace telemetry_detail {

// Returns true if a whitespace-delimited token looks like a filesystem path. Used to redact paths
// (which embed usernames / directory layout) from telemetry strings.
inline bool LooksLikePath(std::string_view token) {
  if (token.find('\\') != std::string_view::npos) {
    return true;  // any backslash: Windows path / UNC
  }
  if (token.size() >= 3 && std::isalpha(static_cast<unsigned char>(token[0])) && token[1] == ':' &&
      (token[2] == '\\' || token[2] == '/')) {
    return true;  // drive-letter prefix: C:\ or C:/
  }
  if (token.size() >= 2 && token[0] == '~' && (token[1] == '/' || token[1] == '\\')) {
    return true;  // home-relative: ~/ or ~\.
  }
  int segments = 0;  // count "/x" runs; 2+ indicates a multi-segment POSIX path
  for (size_t k = 0; k + 1 < token.size(); ++k) {
    if (token[k] == '/' && token[k + 1] != '/') {
      ++segments;
    }
  }
  return segments >= 2;
}

}  // namespace telemetry_detail

// Maximum transmitted telemetry-string length, applied after scrubbing to bound telemetry payload size.
inline constexpr size_t kMaxTelemetryStringLength = 256;

namespace telemetry_detail {

// Replace a token that LooksLikePath() with a "[path]" placeholder that keeps the last two path
// segments (the immediate parent directory and file name) for debuggability. The kept tail is clamped
// so it can never include a home-directory user name — the "<user>" in ~/, /home/<user>/,
// /Users/<user>/, /root/, or C:\Users\<user>\ — which is the value the redaction exists to hide.
// The home markers are matched anywhere in the token, so an embedded path (e.g. "input:/home/u/f")
// is guarded too.
inline std::string RedactPathToken(std::string_view token) {
  const auto is_sep = [](char c) { return c == '/' || c == '\\'; };

  // Case- and separator-insensitive marker matching: home directories on Windows/macOS are
  // case-insensitive, and one path may mix '/' and '\'. Markers are searched in a normalized copy
  // (lowercased, all separators as '/'); ASCII lowercasing preserves length, so its indices map 1:1
  // to the original token, from which the separators and the kept tail are read.
  std::string norm(token);
  for (char& c : norm) {
    c = (c == '\\') ? '/' : static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
  }

  // safe_start: no content before this index may be kept. Advance it past any home prefix + user name.
  size_t safe_start = 0;
  const auto guard = [&](std::string_view marker, bool has_user) {
    for (size_t p = norm.find(marker); p != std::string::npos; p = norm.find(marker, p + 1)) {
      size_t e = p + marker.size();
      if (has_user) {
        // Skip separators and "." (current-dir) segments so path-equivalent spellings such as
        // /home//user or /home/./user still keep the user name out of the tail, then consume the
        // real user-name segment up to its trailing separator.
        while (e < token.size() &&
               (is_sep(token[e]) ||
                (token[e] == '.' && (e + 1 == token.size() || is_sep(token[e + 1]))))) {
          ++e;
        }
        while (e < token.size() && !is_sep(token[e])) {
          ++e;  // consume <user>, stopping at its trailing separator (or end of token)
        }
      } else {
        --e;  // markers with no user segment: keep from the marker's own trailing separator
      }
      if (e > safe_start) {
        safe_start = e;
      }
    }
  };
  guard("/home/", true);
  guard("/users/", true);  // normalized token also matches /Users/, \Users\, \users\, mixed forms
  guard("/root/", false);
  guard("~/", false);  // normalized token also matches ~\.

  // tail_start: start of the last two segments (the second-to-last separator). Trailing separators
  // are ignored first, so a path ending in a separator still keeps two real segments instead of
  // dropping one. With a single separator only one segment is kept; with none, nothing beyond
  // "[path]" is kept. (safe_start is unaffected, so the user-name guard still holds.)
  size_t scan_end = token.size();
  while (scan_end > 0 && is_sep(token[scan_end - 1])) {
    --scan_end;
  }
  size_t tail_start = token.size();
  if (scan_end > 0) {
    const size_t last_sep = token.find_last_of("/\\", scan_end - 1);
    if (last_sep != std::string_view::npos) {
      const size_t prev =
          (last_sep == 0) ? std::string_view::npos : token.find_last_of("/\\", last_sep - 1);
      tail_start = (prev == std::string_view::npos) ? last_sep : prev;
    }
  }

  const size_t keep_from = (tail_start > safe_start) ? tail_start : safe_start;
  std::string out = "[path]";
  if (keep_from < token.size()) {
    out.append(token.data() + keep_from, token.size() - keep_from);
  }
  return out;
}

}  // namespace telemetry_detail

// Scrub filesystem paths out of a free-text telemetry string before transmission and cap its length.
// Each whitespace-delimited token that looks like a path is replaced with a "[path]" placeholder that
// retains the last two path segments (parent directory + file name) for debuggability, while still
// redacting the sensitive prefix — in particular any home directory + user name (e.g. /home/<name>/
// or C:\Users\<name>\), which is never kept.
inline std::string ScrubStringForTelemetry(std::string_view msg) {
  using telemetry_detail::LooksLikePath;
  using telemetry_detail::RedactPathToken;

  std::string out;
  out.reserve(msg.size());

  size_t i = 0;
  while (i < msg.size()) {
    if (std::isspace(static_cast<unsigned char>(msg[i]))) {
      out.push_back(msg[i]);
      ++i;
      continue;
    }
    const size_t start = i;
    while (i < msg.size() && !std::isspace(static_cast<unsigned char>(msg[i]))) {
      ++i;
    }
    const std::string_view token = msg.substr(start, i - start);
    if (LooksLikePath(token)) {
      out += RedactPathToken(token);
    } else {
      out.append(token.data(), token.size());
    }
  }

  if (out.size() > kMaxTelemetryStringLength) {
    out.resize(kMaxTelemetryStringLength);
  }
  return out;
}

}  // namespace onnxruntime
